/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").  

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

/*
 ====================
 GL_SelectTextureNoClient
 ====================
 */
static void GL_SelectTextureNoClient( int unit ) {
    backEnd.glState.currenttmu = unit;
    qglActiveTextureARB( GL_TEXTURE0_ARB + unit );
    RB_LogComment( "glActiveTextureARB( %i )\n", unit );
}

/*
=========================================================================================

GENERAL FORWARD INTERACTION w/ STENCIL SHADOWS RENDERING

=========================================================================================
*/

/*
==================
RB_GLSL_ForwardDrawInteraction
==================
*/
static void RB_GLSL_ForwardDrawInteraction( const drawInteraction_t *din ) {
    static const float zero[4] = { 0, 0, 0, 0 };
    static const float one[4] = { 1, 1, 1, 1 };
    static const float negOne[4] = { -1, -1, -1, -1 };
    interactionParms_t *parms;
    
    // Set up the program uniforms
    parms = &backEnd.interactionParms;
    
    // set matrices
    R_UniformVector4( parms->bumpMatrixS, din->bumpMatrix[0] );
    R_UniformVector4( parms->bumpMatrixT, din->bumpMatrix[1] );
    R_UniformVector4( parms->diffuseMatrixS, din->diffuseMatrix[0] );
    R_UniformVector4( parms->diffuseMatrixT, din->diffuseMatrix[1] );
    R_UniformVector4( parms->specularMatrixS, din->specularMatrix[0] );
    R_UniformVector4( parms->specularMatrixT, din->specularMatrix[1] );

    // set the light parameters
    R_UniformVector4( parms->localLightOrigin, din->localLightOrigin );
    R_UniformVector4( parms->localViewOrigin, din->localViewOrigin );
    
    R_UniformVector4( parms->lightProjectionS, din->lightProjection[0] );
    R_UniformVector4( parms->lightProjectionT, din->lightProjection[1] );
    R_UniformVector4( parms->lightProjectionQ, din->lightProjection[2] );
    R_UniformVector4( parms->lightFalloff, din->lightProjection[3] );

    // set the vertex color modifier
	switch ( din->vertexColor ) {
		case SVC_IGNORE:
            R_UniformFloat4( parms->colorModulate, zero[0], zero[1], zero[2], zero[3] );
            R_UniformFloat4( parms->colorAdd, one[0], one[1], one[2], one[3] );
			break;
		case SVC_MODULATE:
            R_UniformFloat4( parms->colorModulate, one[0], one[1], one[2], one[3] );
            R_UniformFloat4( parms->colorAdd, zero[0], zero[1], zero[2], zero[3] );
			break;
		case SVC_INVERSE_MODULATE:
            R_UniformFloat4( parms->colorModulate, negOne[0], negOne[1], negOne[2], negOne[3] );
            R_UniformFloat4( parms->colorAdd, one[0], one[1], one[2], one[3] );
			break;
	}
	
	// set the color modifiers
    R_UniformVector4( parms->diffuseColor, din->diffuseColor );
    R_UniformVector4( parms->specularColor, din->specularColor );

	// set the textures

	// texture 0 will be the per-surface bump map
	GL_SelectTextureNoClient( 0 );
	din->bumpImage->Bind();

	// texture 1 will be the light falloff texture
	GL_SelectTextureNoClient( 1 );
	din->lightFalloffImage->Bind();

	// texture 2 will be the light projection texture
	GL_SelectTextureNoClient( 2 );
	din->lightImage->Bind();

	// texture 3 is the per-surface diffuse map
	GL_SelectTextureNoClient( 3 );
	din->diffuseImage->Bind();

	// texture 4 is the per-surface specular map
	GL_SelectTextureNoClient( 4 );
	din->specularImage->Bind();

	// draw it
	RB_DrawElementsWithCounters( din->surf->geo );
}


/*
=============
RB_GLSL_CreateForwardDrawInteractions
=============
*/
static void RB_GLSL_CreateForwardDrawInteractions( const drawSurf_t *surf ) {
	if ( !surf ) {
		return;
	}

	// perform setup here that will be constant for all interactions
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc );

	// bind the shader program
	GL_BindProgram( tr.interactionProgram );

	// enable the vertex arrays
	qglEnableVertexAttribArrayARB( 8 );
	qglEnableVertexAttribArrayARB( 9 );
	qglEnableVertexAttribArrayARB( 10 );
	qglEnableVertexAttribArrayARB( 11 );
	qglEnableClientState( GL_COLOR_ARRAY );

	for ( ; surf ; surf=surf->nextOnLight ) {
		// perform setup here that will not change over multiple interaction passes

		// set the vertex pointers
		idDrawVert	*ac = (idDrawVert *)vertexCache.Position( surf->geo->ambientCache );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( idDrawVert ), ac->color );
		qglVertexAttribPointerARB( 11, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
		qglVertexAttribPointerARB( 10, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[1].ToFloatPtr() );
		qglVertexAttribPointerARB( 9, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[0].ToFloatPtr() );
		qglVertexAttribPointerARB( 8, 2, GL_FLOAT, false, sizeof( idDrawVert ), ac->st.ToFloatPtr() );
		qglVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );

		// this may cause RB_GLSL_DrawInteraction to be executed multiple
		// times with different colors and images if the surface or light have multiple layers
		RB_CreateSingleDrawInteractions( surf, RB_GLSL_ForwardDrawInteraction );
	}

	qglDisableVertexAttribArrayARB( 8 );
	qglDisableVertexAttribArrayARB( 9 );
	qglDisableVertexAttribArrayARB( 10 );
	qglDisableVertexAttribArrayARB( 11 );
	qglDisableClientState( GL_COLOR_ARRAY );

	// disable features
	GL_SelectTextureNoClient( 4 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 3 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 2 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 1 );
	globalImages->BindNull();

	backEnd.glState.currenttmu = -1;
	GL_SelectTexture( 0 );

	GL_BindProgram( 0 );
}


/*
==================
RB_GLSL_DrawForwardInteractions
==================
*/
void RB_GLSL_DrawForwardInteractions( void ) {
	viewLight_t		*vLight;

	GL_SelectTexture( 0 );
	qglDisableClientState( GL_TEXTURE_COORD_ARRAY );

	//
	// for each light, perform adding and shadowing
	//
	for ( vLight = backEnd.viewDef->viewLights ; vLight ; vLight = vLight->next ) {
		backEnd.vLight = vLight;

		// do fogging later
		if ( vLight->lightShader->IsFogLight() ) {
			continue;
		}
		if ( vLight->lightShader->IsBlendLight() ) {
			continue;
		}

		// if there are no interactions, get out!
		if ( !vLight->localInteractions && !vLight->globalInteractions && 
			!vLight->translucentInteractions ) {
			continue;
		}

		// clear the stencil buffer if needed
		if ( vLight->globalShadows || vLight->localShadows ) {
			backEnd.currentScissor = vLight->scissorRect;
			if ( r_useScissor.GetBool() ) {
				qglScissor( backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1, 
					backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
					backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
					backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 );
			}
			qglClear( GL_STENCIL_BUFFER_BIT );
		} else {
			// no shadows, so no need to read or write the stencil buffer
			// we might in theory want to use GL_ALWAYS instead of disabling
			// completely, to satisfy the invarience rules
			qglStencilFunc( GL_ALWAYS, 128, 255 );
		}

		RB_StencilShadowPass( vLight->globalShadows );
		RB_GLSL_CreateForwardDrawInteractions( vLight->localInteractions );

		RB_StencilShadowPass( vLight->localShadows );
		RB_GLSL_CreateForwardDrawInteractions( vLight->globalInteractions );

		// translucent surfaces never get stencil shadowed
		if ( r_skipTranslucent.GetBool() ) {
			continue;
		}

		qglStencilFunc( GL_ALWAYS, 128, 255 );

		backEnd.depthFunc = GLS_DEPTHFUNC_LESS;
		RB_GLSL_CreateForwardDrawInteractions( vLight->translucentInteractions );
		backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;
	}

	// disable stencil shadow test
	qglStencilFunc( GL_ALWAYS, 128, 255 );

	GL_SelectTexture( 0 );
	qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
}


