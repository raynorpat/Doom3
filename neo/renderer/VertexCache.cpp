/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"


static const int	FRAME_MEMORY_BYTES = 0x200000;
static const int	EXPAND_HEADERS = 32;

idCVar idVertexCache::r_showVertexCache( "r_showVertexCache", "0", CVAR_INTEGER|CVAR_RENDERER, "show vertex cache" );
idCVar idVertexCache::r_useArbBufferRange( "r_useArbBufferRange", "1", CVAR_BOOL|CVAR_RENDERER, "use ARB_map_buffer_range for optimization" );
idCVar idVertexCache::r_reuseVertexCacheSooner( "r_reuseVertexCacheSooner", "1", CVAR_BOOL|CVAR_RENDERER, "reuse vertex buffers as soon as possible after freeing" );

idVertexCache		vertexCache;

static GLuint gl_current_array_buffer = 0;
static GLuint gl_current_index_buffer = 0;

/*
==============
R_ListVertexCache_f
==============
*/
static void R_ListVertexCache_f( const idCmdArgs &args ) {
	vertexCache.List();
}

/*
 ==============
 GL_BindBuffer
 ==============
 */
static void GL_BindBuffer( GLenum target, GLuint buffer ) {
    if ( target == GL_ARRAY_BUFFER ) {
        if ( gl_current_array_buffer != buffer ) {
            gl_current_array_buffer = buffer;
        } else {
            return;
        }
    } else if ( target == GL_ELEMENT_ARRAY_BUFFER ) {
        if ( gl_current_index_buffer != buffer ) {
            gl_current_index_buffer = buffer;
        } else {
            return;
        }
    } else {
        common->Error( "GL_BindBuffer : invalid buffer target : %i\n", (int) target );
        return;
    }
    
    qglBindBufferARB( target, buffer );
}


/*
==============
idVertexCache::ActuallyFree
==============
*/
void idVertexCache::ActuallyFree( vertCache_t *block ) {
	if (!block) {
		common->Error( "idVertexCache Free: NULL pointer" );
	}

	if ( block->user ) {
		// let the owner know we have purged it
		*block->user = NULL;
		block->user = NULL;
	}

	// temp blocks are in a shared space that won't be freed
	if ( block->tag != TAG_TEMP ) {
		this->staticAllocTotal -= block->size;
		this->staticCountTotal--;
        if ( block->vbo ) {
            // Does not seem to hurt any and is actually used in all other
            // implementations in some form. Changed a bit to map to NULL pointer
            // with block size. Removing this is probably ok but you cannot remove
            // the if ( block->vbo ) cause then it will crash.
            GL_BindBuffer(GL_ARRAY_BUFFER_ARB, block->vbo);
            qglBufferDataARB(GL_ARRAY_BUFFER_ARB, block->size, NULL, GL_DYNAMIC_DRAW_ARB);
        } else if ( block->virtMem ) {
			Mem_Free( block->virtMem );
			block->virtMem = NULL;
		}
	}
	block->tag = TAG_FREE;		// mark as free

	// unlink stick it back on the free list
	block->next->prev = block->prev;
	block->prev->next = block->next;

    if ( r_reuseVertexCacheSooner.GetBool() ) {
        // stick it on the front of the free list so it will be reused immediately
        block->next = this->freeStaticHeaders.next;
        block->prev = &this->freeStaticHeaders;
    } else {
        // stick it on the back of the free list so it won't be reused soon (just for debugging)
        block->next = &this->freeStaticHeaders;
        block->prev = this->freeStaticHeaders.prev;
    }

	block->next->prev = block;
	block->prev->next = block;
}

/*
==============
idVertexCache::Position

this will be a real pointer with virtual memory,
but it will be an int offset cast to a pointer with
ARB_vertex_buffer_object

The ARB_vertex_buffer_object will be bound
==============
*/
void *idVertexCache::Position( vertCache_t *buffer ) {
	if ( !buffer || buffer->tag == TAG_FREE ) {
		common->FatalError( "idVertexCache::Position: bad vertCache_t" );
	}

	// the ARB vertex object just uses an offset
	if ( buffer->vbo ) {
		if ( r_showVertexCache.GetInteger() == 2 ) {
			if ( buffer->tag == TAG_TEMP ) {
				common->Printf( "GL_ARRAY_BUFFER_ARB = %i + %i (%i bytes)\n", buffer->vbo, buffer->offset, buffer->size ); 
			} else {
				common->Printf( "GL_ARRAY_BUFFER_ARB = %i (%i bytes)\n", buffer->vbo, buffer->size ); 
			}
		}
		
        GL_BindBuffer( (buffer->indexBuffer ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER), buffer->vbo );
		return (void *)buffer->offset;
	}

	// virtual memory is a real pointer
	return (void *)((byte *)buffer->virtMem + buffer->offset);
}

/*
 ==============
 idVertexCache::UnbindIndex
 ==============
 */
void idVertexCache::UnbindIndex() {
	GL_BindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
}


//================================================================================

/*
===========
idVertexCache::Init
===========
*/
void idVertexCache::Init() {
	cmdSystem->AddCommand( "listVertexCache", R_ListVertexCache_f, CMD_FL_RENDERER, "lists vertex cache" );

	virtualMemory = false;

	// use ARB_vertex_buffer_object unless explicitly disabled
	if( r_useVertexBuffers.GetInteger() && glConfig.ARBVertexBufferObjectAvailable ) {
		common->Printf( "using ARB_vertex_buffer_object memory\n" );
	} else {
		virtualMemory = true;
		r_useIndexBuffers.SetBool( false );
		common->Printf( "WARNING: vertex array range in virtual memory (SLOW)\n" );
	}

	// initialize the cache memory blocks
	this->freeStaticHeaders.next = this->freeStaticHeaders.prev = &this->freeStaticHeaders;
    staticHeaders.next = staticHeaders.prev = &staticHeaders;
	freeDynamicHeaders.next = freeDynamicHeaders.prev = &freeDynamicHeaders;
	dynamicHeaders.next = dynamicHeaders.prev = &dynamicHeaders;
	deferredFreeList.next = deferredFreeList.prev = &deferredFreeList;

	// set up the dynamic frame memory
	frameBytes = FRAME_MEMORY_BYTES;
	this->staticAllocTotal = 0;

	byte	*junk = (byte *)Mem_Alloc( frameBytes );
	for ( int i = 0 ; i < NUM_VERTEX_FRAMES ; i++ ) {
		this->allocatingTempBuffer = true;      // force the alloc to use GL_STREAM_DRAW_ARB
		this->Alloc( junk, this->frameBytes, &this->tempBuffers[i] );
		this->allocatingTempBuffer = false;
		this->tempBuffers[i]->tag = TAG_FIXED;
        
		// unlink these from the static list, so they won't ever get purged
		this->tempBuffers[i]->next->prev = this->tempBuffers[i]->prev;
		this->tempBuffers[i]->prev->next = this->tempBuffers[i]->next;
	}
	Mem_Free( junk );

	EndFrame();
}

/*
===========
idVertexCache::PurgeAll

Used when toggling vertex programs on or off, because
the cached data isn't valid
===========
*/
void idVertexCache::PurgeAll() {
	while( staticHeaders.next != &staticHeaders ) {
		ActuallyFree( staticHeaders.next );
	}
}

/*
===========
idVertexCache::Shutdown
===========
*/
void idVertexCache::Shutdown() {
	headerAllocator.Shutdown();
}

/*
===========
idVertexCache::Alloc
===========
*/
void idVertexCache::Alloc( void *data, int size, vertCache_t **buffer, bool indexBuffer ) {
	vertCache_t	*block = NULL;

	if ( size <= 0 ) {
		common->Error( "idVertexCache::Alloc: size = %i\n", size );
	}

	// if we can't find anything, it will be NULL
	*buffer = NULL;

	// if we don't have any remaining unused headers, allocate some more
	if ( this->freeStaticHeaders.next == &this->freeStaticHeaders ) {
		for ( int i = 0; i < EXPAND_HEADERS; i++ ) {
			block = headerAllocator.Alloc ();

			if( !virtualMemory ) {
				qglGenBuffersARB( 1, &block->vbo );
                block->size = 0;
			}
            block->next = this->freeStaticHeaders.next;
            block->prev = &this->freeStaticHeaders;
            block->next->prev = block;
            block->prev->next = block;
		}
	}
    
    GLenum target = ( indexBuffer ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER );
    GLenum usage = ( allocatingTempBuffer ? GL_STREAM_DRAW : GL_STATIC_DRAW );

    // try to find a matching block to replace so that we're not continually respecifying vbo data each frame
    for ( vertCache_t *findblock = this->freeStaticHeaders.next; ; findblock = findblock->next ) {
        if ( findblock == &this->freeStaticHeaders ) {
            block = this->freeStaticHeaders.next;
            break;
        }

        if ( findblock->target != target )
            continue;
        if ( findblock->usage != usage )
            continue;
        if ( findblock->size != size )
            continue;

        block = findblock;
        break;
    }

	// move it from the freeStaticHeaders list to the staticHeaders list
    block->target = target;
    block->usage = usage;

    if ( block->vbo ) {
        // orphan the buffer in case it needs respecifying (it usually will)
        GL_BindBuffer( target, block->vbo );
        qglBufferDataARB( target, (GLsizeiptr) size, NULL, usage );
        qglBufferDataARB( target, (GLsizeiptr) size, data, usage );
    } else {
        block->virtMem = Mem_Alloc( size );
        SIMDProcessor->Memcpy( block->virtMem, data, size );
    }
	block->next->prev = block->prev;
	block->prev->next = block->next;
	block->next = staticHeaders.next;
	block->prev = &staticHeaders;
	block->next->prev = block;
	block->prev->next = block;

	block->size = size;
	block->offset = 0;
	block->tag = TAG_USED;

	// save data for debugging
    this->staticAllocThisFrame += block->size;
    this->staticCountThisFrame++;
    this->staticCountTotal++;
    this->staticAllocTotal += block->size;

	// this will be set to zero when it is purged
	block->user = buffer;
	*buffer = block;

	// allocation doesn't imply used-for-drawing, because at level
	// load time lots of things may be created, but they aren't
	// referenced by the GPU yet, and can be purged if needed.
	block->frameUsed = currentFrame - NUM_VERTEX_FRAMES;
	block->indexBuffer = indexBuffer;
}

/*
===========
idVertexCache::Touch
===========
*/
void idVertexCache::Touch( vertCache_t *block ) {
	if ( !block ) {
		common->Error( "idVertexCache Touch: NULL pointer" );
	}

	if ( block->tag == TAG_FREE ) {
		common->FatalError( "idVertexCache Touch: freed pointer" );
	}
	if ( block->tag == TAG_TEMP ) {
		common->FatalError( "idVertexCache Touch: temporary pointer" );
	}

	block->frameUsed = currentFrame;

	// move to the head of the LRU list
	block->next->prev = block->prev;
	block->prev->next = block->next;

	block->next = staticHeaders.next;
	block->prev = &staticHeaders;
    
	staticHeaders.next->prev = block;
	staticHeaders.next = block;
}

/*
===========
idVertexCache::Free
===========
*/
void idVertexCache::Free( vertCache_t *block ) {
	if (!block) {
		return;
	}

	if ( block->tag == TAG_FREE ) {
		common->FatalError( "idVertexCache Free: freed pointer" );
	}
	if ( block->tag == TAG_TEMP ) {
		common->FatalError( "idVertexCache Free: temporary pointer" );
	}

	// this block still can't be purged until the frame count has expired,
	// but it won't need to clear a user pointer when it is
	block->user = NULL;

	block->next->prev = block->prev;
	block->prev->next = block->next;

	block->next = deferredFreeList.next;
	block->prev = &deferredFreeList;
    
	deferredFreeList.next->prev = block;
	deferredFreeList.next = block;
}

/*
===========
idVertexCache::AllocFrameTemp

A frame temp allocation must never be allowed to fail due to overflow.
We can't simply sync with the GPU and overwrite what we have, because
there may still be future references to dynamically created surfaces.
===========
*/
vertCache_t	*idVertexCache::AllocFrameTemp( void *data, int size ) {
	vertCache_t	*block;

	if ( size <= 0 ) {
		common->Error( "idVertexCache::AllocFrameTemp: size = %i\n", size );
	}

	if ( dynamicAllocThisFrame + size > frameBytes ) {
		// if we don't have enough room in the temp block, allocate a static block,
		// but immediately free it so it will get freed at the next frame
        this->tempOverflow = true;
        this->Alloc( data, size, &block );
        this->Free( block);
        return block;
	}

	// this data is just going on the shared dynamic list

	// if we don't have any remaining unused headers, allocate some more
	if ( freeDynamicHeaders.next == &freeDynamicHeaders ) {

		for ( int i = 0; i < EXPAND_HEADERS; i++ ) {
			block = headerAllocator.Alloc();
			block->next = freeDynamicHeaders.next;
			block->prev = &freeDynamicHeaders;
			block->next->prev = block;
			block->prev->next = block;
		}
	}

	// move it from the freeDynamicHeaders list to the dynamicHeaders list
	block = freeDynamicHeaders.next;
	block->next->prev = block->prev;
	block->prev->next = block->next;
	block->next = dynamicHeaders.next;
	block->prev = &dynamicHeaders;
	block->next->prev = block;
	block->prev->next = block;

	block->size = size;
	block->tag = TAG_TEMP;
	block->indexBuffer = false;
	block->offset = dynamicAllocThisFrame;
	dynamicAllocThisFrame += block->size;
	dynamicCountThisFrame++;
	block->user = NULL;
	block->frameUsed = 0;

	// copy the data
	block->virtMem = tempBuffers[listNum]->virtMem;

    if ( ( block->vbo = tempBuffers[listNum]->vbo ) != 0 ) {
        GL_BindBuffer( GL_ARRAY_BUFFER, block->vbo );

        // try to get an unsynchronized map if at all possible
        if ( glConfig.ARBMapBufferRangeAvailable && r_useArbBufferRange.GetBool() ) {
            void *dst = NULL;
            GLbitfield access = GL_MAP_WRITE_BIT|GL_MAP_UNSYNCHRONIZED_BIT|GL_MAP_INVALIDATE_RANGE_BIT;

            // if the buffer has wrapped then we orphan it
            if ( block->offset == 0 ) {
                access = GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT;
            } else {
                access = GL_MAP_WRITE_BIT|GL_MAP_UNSYNCHRONIZED_BIT|GL_MAP_INVALIDATE_RANGE_BIT;
            }
            if ( (dst = qglMapBufferRange(GL_ARRAY_BUFFER, block->offset, (GLsizeiptr) size, access)) != NULL ) {
                SIMDProcessor->Memcpy( (byte *)dst, data, size );
                qglUnmapBufferARB( GL_ARRAY_BUFFER );
                return block;
            } else {
                qglBufferSubDataARB( GL_ARRAY_BUFFER, block->offset, (GLsizeiptr) size, data );
            }
        } else {
            qglBufferSubDataARB( GL_ARRAY_BUFFER, block->offset, (GLsizeiptr) size, data );
        }
	} else {
		SIMDProcessor->Memcpy( (byte *)block->virtMem + block->offset, data, size );
	}
	return block;
}

/*
===========
idVertexCache::EndFrame
===========
*/
void idVertexCache::EndFrame() {
	// display debug information
	if ( r_showVertexCache.GetBool() ) {
		int	staticUseCount = 0;
		int staticUseSize = 0;

		for ( vertCache_t *block = staticHeaders.next ; block != &staticHeaders ; block = block->next ) {
			if ( block->frameUsed == currentFrame ) {
				staticUseCount++;
				staticUseSize += block->size;
			}
		}

		const char *frameOverflow = tempOverflow ? "(OVERFLOW)" : "";

		common->Printf( "vertex dynamic:%i=%ik%s, static alloc:%i=%ik used:%i=%ik total:%i=%ik\n",
			dynamicCountThisFrame, dynamicAllocThisFrame/1024, frameOverflow,
			this->staticCountThisFrame, staticAllocThisFrame/1024,
			staticUseCount, staticUseSize/1024,
			this->staticCountTotal, staticAllocTotal/1024 );
	}

	if( !virtualMemory ) {
		// unbind vertex buffers so normal virtual memory will be used in case
		// r_useVertexBuffers / r_useIndexBuffers
		qglBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		qglBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
	}

	currentFrame = tr.frameCount;
	listNum = currentFrame % NUM_VERTEX_FRAMES;
	this->staticAllocThisFrame = 0;
	this->staticCountThisFrame = 0;
	dynamicAllocThisFrame = 0;
	dynamicCountThisFrame = 0;
	tempOverflow = false;

	// free all the deferred free headers
	while( deferredFreeList.next != &deferredFreeList ) {
		ActuallyFree( deferredFreeList.next );
	}

	// free all the frame temp headers
	vertCache_t	*block = dynamicHeaders.next;
	if ( block != &dynamicHeaders ) {
		block->prev = &freeDynamicHeaders;
		dynamicHeaders.prev->next = freeDynamicHeaders.next;
		freeDynamicHeaders.next->prev = dynamicHeaders.prev;
		freeDynamicHeaders.next = block;

		dynamicHeaders.next = dynamicHeaders.prev = &dynamicHeaders;
	}
}

/*
=============
idVertexCache::List
=============
*/
void idVertexCache::List( void ) {
	int	numActive = 0;
	int frameStatic = 0;
	int	totalStatic = 0;

	vertCache_t *block;
	for ( block = staticHeaders.next ; block != &staticHeaders ; block = block->next) {
		numActive++;

		totalStatic += block->size;
		if ( block->frameUsed == currentFrame ) {
			frameStatic += block->size;
		}
	}

	int	numFreeStaticHeaders = 0;
	for ( block = freeStaticHeaders.next ; block != &freeStaticHeaders ; block = block->next ) {
		numFreeStaticHeaders++;
	}

	int	numFreeDynamicHeaders = 0;
	for ( block = freeDynamicHeaders.next ; block != &freeDynamicHeaders ; block = block->next ) {
		numFreeDynamicHeaders++;
	}

	common->Printf( "%i dynamic temp buffers of %ik\n", NUM_VERTEX_FRAMES, frameBytes / 1024 );
	common->Printf( "%5i active static headers\n", numActive );
	common->Printf( "%5i free static headers\n", numFreeStaticHeaders );
	common->Printf( "%5i free dynamic headers\n", numFreeDynamicHeaders );

	if ( !virtualMemory  ) {
		common->Printf( "Vertex cache is in ARB_vertex_buffer_object memory (FAST).\n");
	} else {
		common->Printf( "Vertex cache is in virtual memory (SLOW)\n" );
	}

	if ( r_useIndexBuffers.GetBool() ) {
		common->Printf( "Index buffers are accelerated.\n" );
	} else {
		common->Printf( "Index buffers are not used.\n" );
	}
}

/*
=============
idVertexCache::IsFast

just for gfxinfo printing
=============
*/
bool idVertexCache::IsFast() {
	if ( virtualMemory ) {
		return false;
	}
	return true;
}
