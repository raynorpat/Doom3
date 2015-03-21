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

/*
 ==============================================================================
 
 BACK END RENDERING OF STENCIL SHADOWS
 
 ==============================================================================
 */

/*
 =====================
 RB_T_Shadow
 
 the shadow volumes face INSIDE
 =====================
 */
static void RB_T_Shadow( const drawSurf_t *surf ) {
    const srfTriangles_t	*tri;
    
    // set the light position in the vertex shader to project the rear surfaces
    if ( surf->space != backEnd.currentSpace ) {
        idVec4 localLight;
        
        // setup stencil shader parms
        stencilShadowParms_t *parms;
        parms = &backEnd.stencilShadowParms;
        
        // grab light origin
        R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.vLight->globalLightOrigin, localLight.ToVec3() );
        localLight.w = 0.0f;
        
        // pass in light origin to vertex shader
        R_UniformVector4( parms->localLightOrigin, localLight );
    }
    
    tri = surf->geo;
    
    if ( !tri->shadowCache ) {
        return;
    }
    
    qglVertexPointer( 4, GL_FLOAT, sizeof( shadowCache_t ), vertexCache.Position(tri->shadowCache) );
    
    // we always draw the sil planes, but we may not need to draw the front or rear caps
    int	numIndexes;
    bool external = false;
    
    if ( !r_useExternalShadows.GetInteger() ) {
        numIndexes = tri->numIndexes;
    } else if ( r_useExternalShadows.GetInteger() == 2 ) { // force to no caps for testing
        numIndexes = tri->numShadowIndexesNoCaps;
    } else if ( !(surf->dsFlags & DSF_VIEW_INSIDE_SHADOW) ) {
        // if we aren't inside the shadow projection, no caps are ever needed needed
        numIndexes = tri->numShadowIndexesNoCaps;
        external = true;
    } else if ( !backEnd.vLight->viewInsideLight && !(surf->geo->shadowCapPlaneBits & SHADOW_CAP_INFINITE) ) {
        // if we are inside the shadow projection, but outside the light, and drawing
        // a non-infinite shadow, we can skip some caps
        if ( backEnd.vLight->viewSeesShadowPlaneBits & surf->geo->shadowCapPlaneBits ) {
            // we can see through a rear cap, so we need to draw it, but we can skip the
            // caps on the actual surface
            numIndexes = tri->numShadowIndexesNoFrontCaps;
        } else {
            // we don't need to draw any caps
            numIndexes = tri->numShadowIndexesNoCaps;
        }
        external = true;
    } else {
        // must draw everything
        numIndexes = tri->numIndexes;
    }
    
    // set depth bounds
    if( glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool() ) {
        qglDepthBoundsEXT( surf->scissorRect.zmin, surf->scissorRect.zmax );
    }
    
    // debug visualization
    if ( r_showShadows.GetInteger() ) {
        if ( r_showShadows.GetInteger() == 3 ) {
            if ( external ) {
                qglColor3f( 0.1, 1, 0.1 );
            } else {
                // these are the surfaces that require the reverse
                qglColor3f( 1, 0.1, 0.1 );
            }
        } else {
            // draw different color for turboshadows
            if ( surf->geo->shadowCapPlaneBits & SHADOW_CAP_INFINITE ) {
                if ( numIndexes == tri->numIndexes ) {
                    qglColor3f( 1, 0.1, 0.1 );
                } else {
                    qglColor3f( 1, 0.4, 0.1 );
                }
            } else {
                if ( numIndexes == tri->numIndexes ) {
                    qglColor3f( 0.1, 1, 0.1 );
                } else if ( numIndexes == tri->numShadowIndexesNoFrontCaps ) {
                    qglColor3f( 0.1, 1, 0.6 );
                } else {
                    qglColor3f( 0.6, 1, 0.1 );
                }
            }
        }
        
        qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
        qglDisable( GL_STENCIL_TEST );
        GL_Cull( CT_TWO_SIDED );
        RB_DrawShadowElementsWithCounters( tri, numIndexes );
        GL_Cull( CT_FRONT_SIDED );
        qglEnable( GL_STENCIL_TEST );
        
        return;
    }
    
    // patent-free work around
    if ( !external ) {
        // depth-fail stencil shadows
        if( r_useTwoSidedStencil.GetBool() && glConfig.twoSidedStencilAvailable ) {
            qglStencilOpSeparate( backEnd.viewDef->isMirror ? GL_FRONT : GL_BACK, GL_KEEP, tr.stencilDecr, GL_KEEP );
            qglStencilOpSeparate( backEnd.viewDef->isMirror ? GL_BACK : GL_FRONT, GL_KEEP, tr.stencilIncr, GL_KEEP );
            GL_Cull( CT_TWO_SIDED );
            RB_DrawShadowElementsWithCounters( tri, numIndexes );
        } else {
            // "preload" the stencil buffer with the number of volumes
            // that get clipped by the near or far clip plane
            qglStencilOp( GL_KEEP, tr.stencilDecr, tr.stencilDecr );
            GL_Cull( CT_FRONT_SIDED );
            RB_DrawShadowElementsWithCounters( tri, numIndexes );
            
            qglStencilOp( GL_KEEP, tr.stencilIncr, tr.stencilIncr );
            GL_Cull( CT_BACK_SIDED );
            RB_DrawShadowElementsWithCounters( tri, numIndexes );
        }
    } else {
        // traditional depth-pass stencil shadows
        if( r_useTwoSidedStencil.GetBool() && glConfig.twoSidedStencilAvailable ) {
            qglStencilOpSeparate( backEnd.viewDef->isMirror ? GL_FRONT : GL_BACK, GL_KEEP, GL_KEEP, tr.stencilIncr );
            qglStencilOpSeparate( backEnd.viewDef->isMirror ? GL_BACK : GL_FRONT, GL_KEEP, GL_KEEP, tr.stencilDecr );
            GL_Cull( CT_TWO_SIDED );
            RB_DrawShadowElementsWithCounters( tri, numIndexes );
        } else {
            qglStencilOp( GL_KEEP, GL_KEEP, tr.stencilIncr );
            GL_Cull( CT_FRONT_SIDED );
            RB_DrawShadowElementsWithCounters( tri, numIndexes );
            
            qglStencilOp( GL_KEEP, GL_KEEP, tr.stencilDecr );
            GL_Cull( CT_BACK_SIDED );
            RB_DrawShadowElementsWithCounters( tri, numIndexes );
        }
    }
}


/*
 =====================
 RB_StencilShadowPass
 
 Stencil test should already be enabled, and the stencil buffer should have
 been set to 128 on any surfaces that might receive shadows
 =====================
 */
void RB_StencilShadowPass( const drawSurf_t *drawSurfs ) {
    if ( !r_shadows.GetBool() ) {
        return;
    }
    
    if ( !drawSurfs ) {
        return;
    }
    
    RB_LogComment( "---------- RB_StencilShadowPass ----------\n" );
    
    GL_BindProgram( tr.stencilShadowProgram );
    
    globalImages->BindNull();
    qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
    
    // for visualizing the shadows
    if ( r_showShadows.GetInteger() ) {
        if ( r_showShadows.GetInteger() == 2 ) {
            // draw filled in
            GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_LESS  );
        } else {
            // draw as lines, filling the depth buffer
            GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_POLYMODE_LINE | GLS_DEPTHFUNC_ALWAYS  );
        }
    } else {
        // don't write to the color buffer, just the stencil buffer
        GL_State( GLS_DEPTHMASK | GLS_COLORMASK | GLS_ALPHAMASK | GLS_DEPTHFUNC_LESS );
    }
    
    if ( r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat() ) {
        qglPolygonOffset( r_shadowPolygonFactor.GetFloat(), -r_shadowPolygonOffset.GetFloat() );
        qglEnable( GL_POLYGON_OFFSET_FILL );
    }
    
    qglStencilFunc( GL_ALWAYS, 1, 255 );
    
    if ( glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool() ) {
        qglEnable( GL_DEPTH_BOUNDS_TEST_EXT );
    }
    
    RB_RenderDrawSurfChainWithFunction( drawSurfs, RB_T_Shadow );
    
    GL_Cull( CT_FRONT_SIDED );
    
    if ( r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat() ) {
        qglDisable( GL_POLYGON_OFFSET_FILL );
    }
    
    if ( glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool() ) {
        qglDisable( GL_DEPTH_BOUNDS_TEST_EXT );
    }
    
    qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
    
    qglStencilFunc( GL_GEQUAL, 128, 255 );
    qglStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
    
    GL_BindProgram( 0 );
}
