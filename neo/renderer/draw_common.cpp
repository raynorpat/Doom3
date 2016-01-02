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
================
RB_SetMVPMatrix
================
*/
void RB_SetMVPMatrix(  const struct viewEntity_s *space  ) {
    float mat[16];
    R_MatrixMultiply( space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat );
    renderProgManager.SetRenderParms( RENDERPARM_MVPMATRIX_X, mat, 4 );
}

static const float zero[4] = { 0, 0, 0, 0 };
static const float one[4] = { 1, 1, 1, 1 };
static const float negOne[4] = { -1, -1, -1, -1 };

/*
================
RB_SetVertexColorParms
================
*/
static void RB_SetVertexColorParms( stageVertexColor_t svc ) {
	switch (svc) {
	case SVC_IGNORE:
		renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_MODULATE, zero );
		renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_ADD, one );
		break;
	case SVC_MODULATE:
		renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_MODULATE, one );
		renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_ADD, zero );
		break;
	case SVC_INVERSE_MODULATE:
		renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_MODULATE, negOne );
		renderProgManager.SetRenderParm( RENDERPARM_VERTEXCOLOR_ADD, one );
		break;
	}
}

/*
=====================
RB_BakeTextureMatrixIntoTexgen
=====================
*/
void RB_BakeTextureMatrixIntoTexgen( idPlane lightProject[3], const float *textureMatrix ) {
	float	genMatrix[16];
	float	final[16];

	genMatrix[0] = lightProject[0][0];
	genMatrix[4] = lightProject[0][1];
	genMatrix[8] = lightProject[0][2];
	genMatrix[12] = lightProject[0][3];

	genMatrix[1] = lightProject[1][0];
	genMatrix[5] = lightProject[1][1];
	genMatrix[9] = lightProject[1][2];
	genMatrix[13] = lightProject[1][3];

	genMatrix[2] = 0;
	genMatrix[6] = 0;
	genMatrix[10] = 0;
	genMatrix[14] = 0;

	genMatrix[3] = lightProject[2][0];
	genMatrix[7] = lightProject[2][1];
	genMatrix[11] = lightProject[2][2];
	genMatrix[15] = lightProject[2][3];

	R_MatrixMultiply( genMatrix, backEnd.lightTextureMatrix, final );

	lightProject[0][0] = final[0];
	lightProject[0][1] = final[4];
	lightProject[0][2] = final[8];
	lightProject[0][3] = final[12];

	lightProject[1][0] = final[1];
	lightProject[1][1] = final[5];
	lightProject[1][2] = final[9];
	lightProject[1][3] = final[13];
}

/*
================
RB_PrepareStageTexturing
================
*/
void RB_PrepareStageTexturing( const shaderStage_t *pStage,  const drawSurf_t *surf, idDrawVert *ac ) {
    float useTexGenParm[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    
	// set privatePolygonOffset if necessary
	if ( pStage->privatePolygonOffset ) {
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset );
	}

	// set the texture matrix if needed
	RB_LoadShaderTextureMatrix( surf->shaderRegisters, &pStage->texture );

	// texgens
    if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {
        // see if there is also a bump map specified
        const shaderStage_t *bumpStage = surf->material->GetBumpStage();
        if ( bumpStage ) {
            // per-pixel reflection mapping with bump mapping
            GL_SelectTexture( 1 );
            bumpStage->texture.image->Bind();
            GL_SelectTexture( 0 );
            
            glVertexAttribPointer( PC_ATTRIB_INDEX_NORMAL, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
            glVertexAttribPointer( PC_ATTRIB_INDEX_BITANGENT, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[1].ToFloatPtr() );
            glVertexAttribPointer( PC_ATTRIB_INDEX_TANGENT, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[0].ToFloatPtr() );
            
            glEnableVertexAttribArray( PC_ATTRIB_INDEX_TANGENT );
            glEnableVertexAttribArray( PC_ATTRIB_INDEX_BITANGENT );
            glEnableVertexAttribArray( PC_ATTRIB_INDEX_NORMAL );
            
            RENDERLOG_PRINTF( "TexGen: TG_REFLECT_CUBE: Bumpy Environment\n" );
            renderProgManager.BindShader_BumpyEnvironment();
        } else {
            // per-pixel reflection mapping without a normal map
            glVertexAttribPointer( PC_ATTRIB_INDEX_NORMAL, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
            glEnableVertexAttribArray( PC_ATTRIB_INDEX_NORMAL );
            
            RENDERLOG_PRINTF( "TexGen: TG_REFLECT_CUBE: Environment\n" );
            renderProgManager.BindShader_Environment();
        }
    }
	else if ( pStage->texture.texgen == TG_SKYBOX_CUBE ) {
        renderProgManager.BindShader_SkyBox();
	}
    else if ( pStage->texture.texgen == TG_WOBBLESKY_CUBE ) {
        const int* parms = surf->material->GetTexGenRegisters();
        
        float wobbleDegrees = surf->shaderRegisters[ parms[0] ] * ( idMath::PI / 180.0f );
        float wobbleSpeed = surf->shaderRegisters[ parms[1] ] * ( 2.0f * idMath::PI / 60.0f );
        float rotateSpeed = surf->shaderRegisters[ parms[2] ] * ( 2.0f * idMath::PI / 60.0f );
        
        idVec3 axis[3];
        {
            // very ad-hoc "wobble" transform
            float s, c;
            idMath::SinCos( wobbleSpeed * backEnd.viewDef->floatTime * 0.001f, s, c );
            
            float ws, wc;
            idMath::SinCos( wobbleDegrees, ws, wc );
            
            axis[2][0] = ws * c;
            axis[2][1] = ws * s;
            axis[2][2] = wc;
            
            axis[1][0] = -s * s * ws;
            axis[1][2] = -s * ws * ws;
            axis[1][1] = idMath::Sqrt( idMath::Fabs( 1.0f - ( axis[1][0] * axis[1][0] + axis[1][2] * axis[1][2] ) ) );
            
            // make the second vector exactly perpendicular to the first
            axis[1] -= ( axis[2] * axis[1] ) * axis[2];
            axis[1].Normalize();
            
            // construct the third with a cross
            axis[0].Cross( axis[1], axis[2] );
        }
        
        // add the rotate
        float rs, rc;
        idMath::SinCos( rotateSpeed * backEnd.viewDef->floatTime * 0.001f, rs, rc );
        
        float transform[12];
        transform[0 * 4 + 0] = axis[0][0] * rc + axis[1][0] * rs;
        transform[0 * 4 + 1] = axis[0][1] * rc + axis[1][1] * rs;
        transform[0 * 4 + 2] = axis[0][2] * rc + axis[1][2] * rs;
        transform[0 * 4 + 3] = 0.0f;
        
        transform[1 * 4 + 0] = axis[1][0] * rc - axis[0][0] * rs;
        transform[1 * 4 + 1] = axis[1][1] * rc - axis[0][1] * rs;
        transform[1 * 4 + 2] = axis[1][2] * rc - axis[0][2] * rs;
        transform[1 * 4 + 3] = 0.0f;
        
        transform[2 * 4 + 0] = axis[2][0];
        transform[2 * 4 + 1] = axis[2][1];
        transform[2 * 4 + 2] = axis[2][2];
        transform[2 * 4 + 3] = 0.0f;
        
        renderProgManager.SetRenderParms( RENDERPARM_WOBBLESKY_X, transform, 3 );
        renderProgManager.BindShader_WobbleSky();
    }
    else if ( pStage->texture.texgen == TG_SCREEN || pStage->texture.texgen == TG_SCREEN2 ) {
        useTexGenParm[0] = 1.0f;
        useTexGenParm[1] = 1.0f;
        useTexGenParm[2] = 1.0f;
        useTexGenParm[3] = 1.0f;
        
        float mat[16];
        R_MatrixMultiply( surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat );
        
        RENDERLOG_PRINTF( "TexGen : %s\n", ( pStage->texture.texgen == TG_SCREEN ) ? "TG_SCREEN" : "TG_SCREEN2" );
        renderLog.Indent();
        
        float plane[4];
        plane[0] = mat[0 * 4 + 0];
        plane[1] = mat[1 * 4 + 0];
        plane[2] = mat[2 * 4 + 0];
        plane[3] = mat[3 * 4 + 0];
        renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_S, plane );
        RENDERLOG_PRINTF( "TEXGEN_S = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );
        
        plane[0] = mat[0 * 4 + 1];
        plane[1] = mat[1 * 4 + 1];
        plane[2] = mat[2 * 4 + 1];
        plane[3] = mat[3 * 4 + 1];
        renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_T, plane );
        RENDERLOG_PRINTF( "TEXGEN_T = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );
        
        plane[0] = mat[0 * 4 + 3];
        plane[1] = mat[1 * 4 + 3];
        plane[2] = mat[2 * 4 + 3];
        plane[3] = mat[3 * 4 + 3];
        renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_Q, plane );
        RENDERLOG_PRINTF( "TEXGEN_Q = %4.3f, %4.3f, %4.3f, %4.3f\n",  plane[0], plane[1], plane[2], plane[3] );
        
        renderLog.Outdent();
	}
    else if( pStage->texture.texgen == TG_DIFFUSE_CUBE )
    {
        // As far as I can tell, this is never used
        idLib::Warning( "Using Diffuse Cube! Please contact Brian!" );
        
    }
    else if( pStage->texture.texgen == TG_GLASSWARP )
    {
        // As far as I can tell, this is never used
        idLib::Warning( "Using GlassWarp! Please contact Brian!" );
    }
    
    renderProgManager.SetRenderParm( RENDERPARM_TEXGEN_0_ENABLED, useTexGenParm );
}

/*
================
RB_FinishStageTexturing
================
*/
void RB_FinishStageTexturing( const shaderStage_t *pStage, const drawSurf_t *surf, idDrawVert *ac ) {
	// unset privatePolygonOffset if necessary
	if ( pStage->privatePolygonOffset && !surf->material->TestMaterialFlag( MF_POLYGONOFFSET ) ) {
		glDisable( GL_POLYGON_OFFSET_FILL );
	}

	if ( pStage->texture.texgen == TG_REFLECT_CUBE ) {
		// see if there is also a bump map specified
		const shaderStage_t *bumpStage = surf->material->GetBumpStage();
		if ( bumpStage ) {
			// per-pixel reflection mapping with bump mapping
			GL_SelectTexture( 1 );
			globalImages->BindNull();
			GL_SelectTexture( 0 );

			glDisableVertexAttribArray( PC_ATTRIB_INDEX_TANGENT );
			glDisableVertexAttribArray( PC_ATTRIB_INDEX_BITANGENT );
		} else {
			// per-pixel reflection mapping without bump mapping
		}
        
        glDisableVertexAttribArray( PC_ATTRIB_INDEX_NORMAL );

		renderProgManager.Unbind();
	}
}

/*
=============================================================================================

FILL DEPTH BUFFER

=============================================================================================
*/


/*
==================
RB_T_FillDepthBuffer
==================
*/
void RB_T_FillDepthBuffer( const drawSurf_t *surf ) {
	int			stage;
	const idMaterial	*shader;
	const shaderStage_t *pStage;
	const float	*regs;
	float		color[4];
	const srfTriangles_t	*tri;

	tri = surf->geo;
	shader = surf->material;

	if ( !shader->IsDrawn() ) {
		return;
	}

	// some deforms may disable themselves by setting numIndexes = 0
	if ( !tri->numIndexes ) {
		return;
	}

	// translucent surfaces don't put anything in the depth buffer and don't
	// test against it, which makes them fail the mirror clip plane operation
	if ( shader->Coverage() == MC_TRANSLUCENT ) {
		return;
	}

	if ( !tri->ambientCache ) {
		common->Printf( "RB_T_FillDepthBuffer: !tri->ambientCache\n" );
		return;
	}

	// get the expressions for conditionals / color / texcoords
	regs = surf->shaderRegisters;

	// if all stages of a material have been conditioned off, don't do anything
	for ( stage = 0; stage < shader->GetNumStages() ; stage++ ) {		
		pStage = shader->GetStage(stage);
		// check the stage enable condition
		if ( regs[ pStage->conditionRegister ] != 0 ) {
			break;
		}
	}
	if ( stage == shader->GetNumStages() ) {
		return;
	}

	// set polygon offset if necessary
	if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
	}

	// subviews will just down-modulate the color buffer by overbright
	if ( shader->GetSort() == SS_SUBVIEW ) {
		GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_LESS );
		color[0] = 1.0f;
		color[1] = 1.0f;
		color[2] = 1.0f;
		color[3] = 1.0f;
	} else {
		// others just draw black
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 1.0f;
	}

	idDrawVert *ac = (idDrawVert *)vertexCache.Position( tri->ambientCache );
    glVertexAttribPointer( PC_ATTRIB_INDEX_VERTEX, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );
    glVertexAttribPointer( PC_ATTRIB_INDEX_ST, 2, GL_FLOAT, false, sizeof( idDrawVert ), reinterpret_cast<void *>(&ac->st) );

	bool drawSolid = false;

	if ( shader->Coverage() == MC_OPAQUE ) {
		drawSolid = true;
	} else if ( shader->Coverage() == MC_PERFORATED ) {
		// we may have multiple alpha tested stages
		// if the only alpha tested stages are condition register omitted,
		// draw a normal opaque surface
		bool	didDraw = false;

		// perforated surfaces may have multiple alpha tested stages
		for ( stage = 0; stage < shader->GetNumStages() ; stage++ ) {		
			pStage = shader->GetStage(stage);

			if ( !pStage->hasAlphaTest ) {
				continue;
			}

			// check the stage enable condition
			if ( regs[ pStage->conditionRegister ] == 0 ) {
				continue;
			}

			// if we at least tried to draw an alpha tested stage,
			// we won't draw the opaque surface
			didDraw = true;

			// set the alpha modulate
			color[3] = regs[ pStage->color.registers[3] ];

			// skip the entire stage if alpha would be black
			if ( color[3] <= 0 ) {
				continue;
			}

			renderProgManager.SetRenderParm( RENDERPARM_COLOR, color );
			renderProgManager.SetRenderParm( RENDERPARM_ALPHA_TEST, &regs[ pStage->alphaTestRegister ] );
			RB_SetVertexColorParms( SVC_IGNORE );

			// bind the texture
			pStage->texture.image->Bind();

			// set texture matrix and texGens
			RB_PrepareStageTexturing( pStage, surf, ac );

			// draw it
			RB_DrawElementsWithCounters( tri );

			// clean up
			RB_FinishStageTexturing( pStage, surf, ac );
		}

		if ( !didDraw ) {
			drawSolid = true;
		}
	}

	// draw the entire surface solid
	if ( drawSolid ) {
		renderProgManager.SetRenderParm( RENDERPARM_COLOR, color );
		renderProgManager.SetRenderParm( RENDERPARM_ALPHA_TEST, vec4_zero.ToFloatPtr() );

		globalImages->whiteImage->Bind();

		// draw it
		RB_DrawElementsWithCounters( tri );
	}

	// reset polygon offset
	if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		glDisable( GL_POLYGON_OFFSET_FILL );
	}

	// reset blending
	if ( shader->GetSort() == SS_SUBVIEW ) {
		GL_State( GLS_DEPTHFUNC_LESS );
	}
}

/*
=====================
RB_STD_FillDepthBuffer

If we are rendering a subview with a near clip plane, use a second texture
to force the alpha test to fail when behind that clip plane
=====================
*/
void RB_STD_FillDepthBuffer( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	// if we are just doing 2D rendering, no need to fill the depth buffer
	if ( !backEnd.viewDef->viewEntitys ) {
		return;
	}

	renderLog.OpenMainBlock( MRB_FILL_DEPTH_BUFFER );
	renderLog.OpenBlock( "RB_FillDepthBuffer" );

	renderProgManager.BindShader_Depth();

	// the first texture will be used for alpha tested surfaces
	GL_SelectTexture( 0 );
	glEnableVertexAttribArray( PC_ATTRIB_INDEX_ST );

	// decal surfaces may enable polygon offset
	glPolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() );

	GL_State( GLS_DEPTHFUNC_LESS );

	// Enable stencil test if we are going to be using it for shadows.
	// If we didn't do this, it would be legal behavior to get z fighting
	// from the ambient pass and the light passes.
	glEnable( GL_STENCIL_TEST );
	glStencilFunc( GL_ALWAYS, 1, 255 );

	RB_RenderDrawSurfListWithFunction( drawSurfs, numDrawSurfs, RB_T_FillDepthBuffer );

	renderProgManager.Unbind();

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

/*
=============================================================================================

SHADER PASSES

=============================================================================================
*/

/*
==================
RB_SetProgramEnvironment

Sets variables that can be used by all vertex programs
==================
*/
void RB_SetProgramEnvironment( void ) {
    // screen power of two correction factor (no longer relevant now)
    float screenCorrectionParm[4];
    screenCorrectionParm[0] = 1.0f;
    screenCorrectionParm[1] = 1.0f;
    screenCorrectionParm[2] = 0.0f;
    screenCorrectionParm[3] = 1.0f;
    renderProgManager.SetRenderParm( RENDERPARM_SCREENCORRECTIONFACTOR, screenCorrectionParm ); // rpScreenCorrectionFactor
    
    // window coord to 0.0 to 1.0 conversion
    int	w = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
    int	h = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;
    float windowCoordParm[4];
    windowCoordParm[0] = 1.0f / w;
    windowCoordParm[1] = 1.0f / h;
    windowCoordParm[2] = 0.0f;
    windowCoordParm[3] = 1.0f;
    renderProgManager.SetRenderParm( RENDERPARM_WINDOWCOORD, windowCoordParm ); // rpWindowCoord
    
    // set eye position in global space
    float parm[4];
    parm[0] = backEnd.viewDef->renderView.vieworg[0];
    parm[1] = backEnd.viewDef->renderView.vieworg[1];
    parm[2] = backEnd.viewDef->renderView.vieworg[2];
    parm[3] = 1.0f;
    renderProgManager.SetRenderParm( RENDERPARM_GLOBALEYEPOS, parm ); // rpGlobalEyePos
    
    // sets overbright to make world brighter
    // This value is baked into the specularScale and diffuseScale values so
    // the interaction programs don't need to perform the extra multiply,
    // but any other renderprogs that want to obey the brightness value
    // can reference this.
    float overbright = r_lightScale.GetFloat() * 0.5f;
    parm[0] = overbright;
    parm[1] = overbright;
    parm[2] = overbright;
    parm[3] = overbright;
    renderProgManager.SetRenderParm( RENDERPARM_OVERBRIGHT, parm );
    
    // Set Projection Matrix
    float projMatrixTranspose[16];
    R_MatrixTranspose( backEnd.viewDef->projectionMatrix, projMatrixTranspose );
    renderProgManager.SetRenderParms( RENDERPARM_PROJMATRIX_X, projMatrixTranspose, 4 );
}

/*
==================
RB_SetProgramEnvironmentSpace

Sets variables related to the current space that can be used by all vertex programs
==================
*/
void RB_SetProgramEnvironmentSpace( void ) {
	const struct viewEntity_s *space = backEnd.currentSpace;
    
    // set eye position in local space
    idVec4 localViewOrigin( 1.0f );
    R_GlobalPointToLocal( space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewOrigin.ToVec3() );
    renderProgManager.SetRenderParm( RENDERPARM_LOCALVIEWORIGIN, localViewOrigin.ToFloatPtr() );
    
    // set model Matrix
    float modelMatrixTranspose[16];
    R_MatrixTranspose( space->modelMatrix, modelMatrixTranspose );
    renderProgManager.SetRenderParms( RENDERPARM_MODELMATRIX_X, modelMatrixTranspose, 4 );
    
    // Set ModelView Matrix
    float modelViewMatrixTranspose[16];
    R_MatrixTranspose( space->modelViewMatrix, modelViewMatrixTranspose );
    renderProgManager.SetRenderParms( RENDERPARM_MODELVIEWMATRIX_X, modelViewMatrixTranspose, 4 );
}

/*
==================
RB_STD_T_RenderShaderPasses

This is also called for the generated 2D rendering
==================
*/
void RB_STD_T_RenderShaderPasses( const drawSurf_t *surf ) {
	int			stage;
	const idMaterial	*shader;
	const shaderStage_t *pStage;
	const float	*regs;
	const srfTriangles_t	*tri;

	tri = surf->geo;
	shader = surf->material;

	if ( !shader->HasAmbient() ) {
		return;
	}

	if ( shader->IsPortalSky() ) {
		return;
	}

	// change the matrix if needed
	if ( surf->space != backEnd.currentSpace ) {
		glLoadMatrixf( surf->space->modelViewMatrix );
        RB_SetMVPMatrix( surf->space );
		backEnd.currentSpace = surf->space;

		RB_SetProgramEnvironmentSpace();
	}

	// change the scissor if needed
	if ( r_useScissor.GetBool() && !backEnd.currentScissor.Equals( surf->scissorRect ) ) {
		backEnd.currentScissor = surf->scissorRect;
		glScissor( backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1, 
			backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
			backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
			backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 );
	}

	// some deforms may disable themselves by setting numIndexes = 0
	if ( !tri->numIndexes ) {
		return;
	}

	if ( !tri->ambientCache ) {
		common->Printf( "RB_T_RenderShaderPasses: !tri->ambientCache\n" );
		return;
	}

	// get the expressions for conditionals / color / texcoords
	regs = surf->shaderRegisters;

	// set face culling appropriately
	GL_Cull( shader->GetCullType() );

	// set polygon offset if necessary
	if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		glEnable( GL_POLYGON_OFFSET_FILL );
		glPolygonOffset( r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset() );
	}
	
	if ( surf->space->weaponDepthHack ) {
		RB_EnterWeaponDepthHack( surf );
	}

	if ( surf->space->modelDepthHack != 0.0f ) {
		RB_EnterModelDepthHack( surf );
	}

	idDrawVert *ac = (idDrawVert *)vertexCache.Position( tri->ambientCache );
    glVertexAttribPointer( PC_ATTRIB_INDEX_VERTEX, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );
    glVertexAttribPointer( PC_ATTRIB_INDEX_ST, 2, GL_FLOAT, false, sizeof( idDrawVert ), reinterpret_cast<void *>(&ac->st) );
    glVertexAttribPointer( PC_ATTRIB_INDEX_COLOR, 4, GL_UNSIGNED_BYTE, false, sizeof( idDrawVert ), (void *)&ac->color );
    glVertexAttribPointer( PC_ATTRIB_INDEX_TANGENT, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[0].ToFloatPtr() );
    glVertexAttribPointer( PC_ATTRIB_INDEX_BITANGENT, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[1].ToFloatPtr() );
    glVertexAttribPointer( PC_ATTRIB_INDEX_NORMAL, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
    
    glEnableVertexAttribArray( PC_ATTRIB_INDEX_COLOR );
    glEnableVertexAttribArray( PC_ATTRIB_INDEX_TANGENT );
    glEnableVertexAttribArray( PC_ATTRIB_INDEX_BITANGENT );
    glEnableVertexAttribArray( PC_ATTRIB_INDEX_NORMAL );
    
	for ( stage = 0; stage < shader->GetNumStages() ; stage++ ) {
		pStage = shader->GetStage(stage);

		// check the enable condition
		if ( regs[ pStage->conditionRegister ] == 0 ) {
			continue;
		}

		// skip the stages involved in lighting
		if ( pStage->lighting != SL_AMBIENT ) {
			continue;
		}

		// skip if the stage is ( GL_ZERO, GL_ONE ), which is used for some alpha masks
		if ( ( pStage->drawStateBits & (GLS_SRCBLEND_BITS|GLS_DSTBLEND_BITS) ) == ( GLS_SRCBLEND_ZERO | GLS_DSTBLEND_ONE ) ) {
			continue;
		}

		// see if we are a new-style stage
		newShaderStage_t *newStage = pStage->newStage;
		if ( newStage ) {
			//--------------------------
			//
			// new style stages
			//
			//--------------------------
			if ( r_skipNewAmbient.GetBool() ) {
				continue;
			}
            renderLog.OpenBlock( "New Shader Stage" );

			GL_State( pStage->drawStateBits );
			
            // RB: CRITICAL BUGFIX: changed newStage->glslProgram to vertexProgram and fragmentProgram
            // otherwise it will result in an out of bounds crash in RB_DrawElementsWithCounters
            renderProgManager.BindShader( newStage->glslProgram, newStage->vertexProgram, newStage->fragmentProgram, false );
            // RB end

#if 0
			// megaTextures bind a lot of images and set a lot of parameters
			if ( newStage->megaTexture ) {
				newStage->megaTexture->SetMappingForSurface( tri );
				idVec3	localViewer;
				R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewer );
				newStage->megaTexture->BindForViewOrigin( localViewer );
			}
#endif

            for( int j = 0; j < newStage->numVertexParms; j++ )
            {
                float parm[4];
                parm[0] = regs[ newStage->vertexParms[j][0] ];
                parm[1] = regs[ newStage->vertexParms[j][1] ];
                parm[2] = regs[ newStage->vertexParms[j][2] ];
                parm[3] = regs[ newStage->vertexParms[j][3] ];
                renderProgManager.SetRenderParm( ( renderParm_t )( RENDERPARM_USER + j ), parm );
            }

            // bind texture units
			for ( int i = 0 ; i < newStage->numFragmentProgramImages ; i++ ) {
				if ( newStage->fragmentProgramImages[i] ) {
					GL_SelectTexture( i );
					newStage->fragmentProgramImages[i]->Bind();
				}
			}

			// draw it
			RB_DrawElementsWithCounters( tri );

            // unbind texture units
			for ( int i = 0 ; i < newStage->numFragmentProgramImages ; i++ ) {
				if ( newStage->fragmentProgramImages[i] ) {
					GL_SelectTexture( i );
					globalImages->BindNull();
				}
			}
#if 0
			if ( newStage->megaTexture ) {
				newStage->megaTexture->Unbind();
			}
#endif

			GL_SelectTexture( 0 );
            renderProgManager.Unbind();

            renderLog.CloseBlock();
			continue;
		}

		//--------------------------
		//
		// old style stages
		//
		//--------------------------

		// set the color
        idVec4 color;
		color[0] = regs[ pStage->color.registers[0] ];
		color[1] = regs[ pStage->color.registers[1] ];
		color[2] = regs[ pStage->color.registers[2] ];
		color[3] = regs[ pStage->color.registers[3] ];

		// skip the entire stage if an add would be black
		if ( ( pStage->drawStateBits & (GLS_SRCBLEND_BITS|GLS_DSTBLEND_BITS) ) == ( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE ) 
			&& color[0] <= 0 && color[1] <= 0 && color[2] <= 0 ) {
			continue;
		}

		// skip the entire stage if a blend would be completely transparent
		if ( ( pStage->drawStateBits & (GLS_SRCBLEND_BITS|GLS_DSTBLEND_BITS) ) == ( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA )
			&& color[3] <= 0 ) {
			continue;
		}
        
        stageVertexColor_t svc = pStage->vertexColor;
        
        renderLog.OpenBlock( "Old Shader Stage" );
        GL_Color( color );
        
        if( ( pStage->texture.texgen == TG_SCREEN ) || ( pStage->texture.texgen == TG_SCREEN2 ) )
        {
            renderProgManager.BindShader_TextureTexGenVertexColor();
        }
        else
        {
            renderProgManager.BindShader_TextureVertexColor();
        }
        
        RB_SetVertexColorParms( svc );

		// bind the texture
		RB_BindVariableStageImage( &pStage->texture, regs );

		// set the state
		GL_State( pStage->drawStateBits );
		
		RB_PrepareStageTexturing( pStage, surf, ac );

		// draw it
		RB_DrawElementsWithCounters( tri );

		RB_FinishStageTexturing( pStage, surf, ac );
		
		renderLog.CloseBlock();
	}
    
    glDisableVertexAttribArray( PC_ATTRIB_INDEX_COLOR );
    glDisableVertexAttribArray( PC_ATTRIB_INDEX_TANGENT );
    glDisableVertexAttribArray( PC_ATTRIB_INDEX_BITANGENT );
    glDisableVertexAttribArray( PC_ATTRIB_INDEX_NORMAL );

	// reset polygon offset
	if ( shader->TestMaterialFlag(MF_POLYGONOFFSET) ) {
		glDisable( GL_POLYGON_OFFSET_FILL );
	}
	if ( surf->space->weaponDepthHack || surf->space->modelDepthHack != 0.0f ) {
		RB_LeaveDepthHack( surf );
	}
}

/*
=====================
RB_STD_DrawShaderPasses

Draw non-light dependent passes
=====================
*/
int RB_STD_DrawShaderPasses( drawSurf_t **drawSurfs, int numDrawSurfs ) {
	int				i;

	// only obey skipAmbient if we are rendering a view
	if ( backEnd.viewDef->viewEntitys && r_skipAmbient.GetBool() ) {
		return numDrawSurfs;
	}

	RENDERLOG_PRINTF( "---------- RB_STD_DrawShaderPasses ----------\n" );

	// if we are about to draw the first surface that needs
	// the rendering in a texture, copy it over
	if ( drawSurfs[0]->material->GetSort() >= SS_POST_PROCESS ) {
		if ( r_skipPostProcess.GetBool() ) {
			return 0;
		}

		// only dump if in a 3d view
		if ( backEnd.viewDef->viewEntitys ) {
			globalImages->currentRenderImage->CopyFramebuffer( backEnd.viewDef->viewport.x1,
				backEnd.viewDef->viewport.y1,  backEnd.viewDef->viewport.x2 -  backEnd.viewDef->viewport.x1 + 1,
				backEnd.viewDef->viewport.y2 -  backEnd.viewDef->viewport.y1 + 1, true );
		}
		backEnd.currentRenderCopied = true;
	}

	GL_SelectTexture( 1 );
	globalImages->BindNull();

	GL_SelectTexture( 0 );
	glEnableVertexAttribArray( PC_ATTRIB_INDEX_ST );

	// we don't use RB_RenderDrawSurfListWithFunction()
	// because we want to defer the matrix load because many
	// surfaces won't draw any ambient passes
	backEnd.currentSpace = NULL;
	for (i = 0  ; i < numDrawSurfs ; i++ ) {
		if ( drawSurfs[i]->material->SuppressInSubview() ) {
			continue;
		}

		if ( backEnd.viewDef->isXraySubview && drawSurfs[i]->space->entityDef ) {
			if ( drawSurfs[i]->space->entityDef->parms.xrayIndex != 2 ) {
				continue;
			}
		}

		// we need to draw the post process shaders after we have drawn the fog lights
		if ( drawSurfs[i]->material->GetSort() >= SS_POST_PROCESS && !backEnd.currentRenderCopied ) {
			break;
		}

		RB_STD_T_RenderShaderPasses( drawSurfs[i] );
	}

	GL_Cull( CT_FRONT_SIDED );
	glColor3f( 1, 1, 1 );

	return i;
}



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
	
	if ( surf->space != backEnd.currentSpace ) {
        RB_SetMVPMatrix( surf->space );
        
        // set the light position to project the rear surfaces
		idVec4 localLight( 0.0f );
		R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.vLight->globalLightOrigin, localLight.ToVec3() );
		renderProgManager.SetRenderParm( RENDERPARM_LOCALLIGHTORIGIN, localLight.ToFloatPtr() );
	}

	tri = surf->geo;

	if ( !tri->shadowCache ) {
		return;
	}

    glVertexAttribPointer( PC_ATTRIB_INDEX_VERTEX, 4, GL_FLOAT, false, sizeof( shadowCache_t ), vertexCache.Position(tri->shadowCache) );

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
		glDepthBoundsEXT( surf->scissorRect.zmin, surf->scissorRect.zmax );
	}

	// debug visualization
	if ( r_showShadows.GetInteger() ) {
		idVec4 debugColor = idVec4( 0.0f, 0.0f, 0.0f, 0.0f );
		if ( r_showShadows.GetInteger() == 3 ) {
			if ( external ) {
				debugColor = idVec4( 0.1f, 1.0f, 0.1f, 1.0f );
			} else {
				// these are the surfaces that require the reverse
				debugColor = idVec4( 1.0f, 0.1f, 0.1f, 1.0f );
			}
		} else {
			// draw different color for turboshadows
			if ( surf->geo->shadowCapPlaneBits & SHADOW_CAP_INFINITE ) {
				if ( numIndexes == tri->numIndexes ) {
					debugColor = idVec4( 1.0f, 0.1f, 0.1f, 1.0f );
				} else {
					debugColor = idVec4( 1.0f, 0.4f, 0.1f, 1.0f );
				}
			} else {
				if ( numIndexes == tri->numIndexes ) {
					debugColor = idVec4( 0.1f, 1.0f, 0.1f, 1.0f );
				} else if ( numIndexes == tri->numShadowIndexesNoFrontCaps ) {
					debugColor = idVec4( 0.1f, 1.0f, 0.6f, 1.0f );
				} else {
					debugColor = idVec4( 0.6f, 1.0f, 0.1f, 1.0f );
				}
			}
		}

		renderProgManager.SetRenderParm( RENDERPARM_COLOR, debugColor.ToFloatPtr() );

		renderProgManager.CommitUniforms();

		glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );
		glDisable( GL_STENCIL_TEST );

		GL_Cull( CT_TWO_SIDED );

		RB_DrawShadowElementsWithCounters( tri, numIndexes );

		GL_Cull( CT_FRONT_SIDED );

		glEnable( GL_STENCIL_TEST );

		return;
	}

	renderProgManager.CommitUniforms();

	// patent-free work around
	if ( !external ) {
		// depth-fail stencil shadows
		if( r_useTwoSidedStencil.GetBool() && glConfig.twoSidedStencilAvailable ) {
			glStencilOpSeparate( backEnd.viewDef->isMirror ? GL_FRONT : GL_BACK, GL_KEEP, tr.stencilDecr, GL_KEEP );
			glStencilOpSeparate( backEnd.viewDef->isMirror ? GL_BACK : GL_FRONT, GL_KEEP, tr.stencilIncr, GL_KEEP );
			GL_Cull( CT_TWO_SIDED );
			RB_DrawShadowElementsWithCounters( tri, numIndexes );
		} else {
			// "preload" the stencil buffer with the number of volumes
			// that get clipped by the near or far clip plane
			glStencilOp( GL_KEEP, tr.stencilDecr, tr.stencilDecr );
			GL_Cull( CT_FRONT_SIDED );
			RB_DrawShadowElementsWithCounters( tri, numIndexes );
	
			glStencilOp( GL_KEEP, tr.stencilIncr, tr.stencilIncr );
			GL_Cull( CT_BACK_SIDED );
			RB_DrawShadowElementsWithCounters( tri, numIndexes );
		}
	} else {
		// traditional depth-pass stencil shadows
		if( r_useTwoSidedStencil.GetBool() && glConfig.twoSidedStencilAvailable ) {
			glStencilOpSeparate( backEnd.viewDef->isMirror ? GL_FRONT : GL_BACK, GL_KEEP, GL_KEEP, tr.stencilIncr );
			glStencilOpSeparate( backEnd.viewDef->isMirror ? GL_BACK : GL_FRONT, GL_KEEP, GL_KEEP, tr.stencilDecr );
			GL_Cull( CT_TWO_SIDED );
			RB_DrawShadowElementsWithCounters( tri, numIndexes );
		} else {	
			glStencilOp( GL_KEEP, GL_KEEP, tr.stencilIncr );
			GL_Cull( CT_FRONT_SIDED );
			RB_DrawShadowElementsWithCounters( tri, numIndexes );
	
			glStencilOp( GL_KEEP, GL_KEEP, tr.stencilDecr );
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

	RENDERLOG_PRINTF("---------- RB_StencilShadowPass ----------\n");

	globalImages->BindNull();
    glDisableVertexAttribArray( PC_ATTRIB_INDEX_ST );

	// for visualizing the shadows
	if ( r_showShadows.GetInteger() ) {
		renderProgManager.BindShader_ShadowDebug();

		if ( r_showShadows.GetInteger() == 2 ) {
			// draw filled in
			GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_LESS  );
		} else {
			// draw as lines, filling the depth buffer
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_POLYMODE_LINE | GLS_DEPTHFUNC_ALWAYS  );
		}
	} else {
		renderProgManager.BindShader_Shadow();

		// don't write to the color buffer, just the stencil buffer
		GL_State( GLS_DEPTHMASK | GLS_COLORMASK | GLS_ALPHAMASK | GLS_DEPTHFUNC_LESS );
	}

	if ( r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat() ) {
		glPolygonOffset( r_shadowPolygonFactor.GetFloat(), -r_shadowPolygonOffset.GetFloat() );
		glEnable( GL_POLYGON_OFFSET_FILL );
	}

	glStencilFunc( GL_ALWAYS, 1, 255 );

	if ( glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool() ) {
		glEnable( GL_DEPTH_BOUNDS_TEST_EXT );
	}

	RB_RenderDrawSurfChainWithFunction( drawSurfs, RB_T_Shadow );

	GL_Cull( CT_FRONT_SIDED );

	if ( r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat() ) {
		glDisable( GL_POLYGON_OFFSET_FILL );
	}

	if ( glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool() ) {
		glDisable( GL_DEPTH_BOUNDS_TEST_EXT );
	}

	glEnableVertexAttribArray( PC_ATTRIB_INDEX_ST );

	glStencilFunc( GL_GEQUAL, 128, 255 );
	glStencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

	renderProgManager.Unbind();
}



/*
=============================================================================================

BLEND LIGHT PROJECTION

=============================================================================================
*/

/*
=====================
RB_T_BlendLight
=====================
*/
static void RB_T_BlendLight( const drawSurf_t *surf ) {
	const srfTriangles_t *tri;

	tri = surf->geo;

	if ( backEnd.currentSpace != surf->space ) {
		idPlane	lightProject[4];
		int		i;

		for ( i = 0 ; i < 4 ; i++ ) {
			R_GlobalPlaneToLocal( surf->space->modelMatrix, backEnd.vLight->lightProject[i], lightProject[i] );
		}

		GL_SelectTexture( 0 );
		glTexGenfv( GL_S, GL_OBJECT_PLANE, lightProject[0].ToFloatPtr() );
		glTexGenfv( GL_T, GL_OBJECT_PLANE, lightProject[1].ToFloatPtr() );
		glTexGenfv( GL_Q, GL_OBJECT_PLANE, lightProject[2].ToFloatPtr() );

		GL_SelectTexture( 1 );
		glTexGenfv( GL_S, GL_OBJECT_PLANE, lightProject[3].ToFloatPtr() );
	}

	// this gets used for both blend lights and shadow draws
	if ( tri->ambientCache ) {
		idDrawVert	*ac = (idDrawVert *)vertexCache.Position( tri->ambientCache );
        glVertexAttribPointer( PC_ATTRIB_INDEX_VERTEX, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );
	} else if ( tri->shadowCache ) {
		shadowCache_t	*sc = (shadowCache_t *)vertexCache.Position( tri->shadowCache );
        glVertexAttribPointer( PC_ATTRIB_INDEX_VERTEX, 3, GL_FLOAT, false, sizeof( shadowCache_t ), sc->xyz.ToFloatPtr() );
	}

	RB_DrawElementsWithCounters( tri );
}


/*
=====================
RB_BlendLight

Dual texture together the falloff and projection texture with a blend
mode to the framebuffer, instead of interacting with the surface texture
=====================
*/
static void RB_BlendLight( const drawSurf_t *drawSurfs,  const drawSurf_t *drawSurfs2 ) {
	const idMaterial	*lightShader;
	const shaderStage_t	*stage;
	int					i;
	const float	*regs;

	if ( !drawSurfs ) {
		return;
	}
	if ( r_skipBlendLights.GetBool() ) {
		return;
	}
	RENDERLOG_PRINTF( "---------- RB_BlendLight ----------\n" );

	lightShader = backEnd.vLight->lightShader;
	regs = backEnd.vLight->shaderRegisters;

	// texture 1 will get the falloff texture
	GL_SelectTexture( 1 );
	glDisableVertexAttribArray( PC_ATTRIB_INDEX_ST );
	glEnable( GL_TEXTURE_GEN_S );
	glTexCoord2f( 0, 0.5 );
	backEnd.vLight->falloffImage->Bind();

	// texture 0 will get the projected texture
	GL_SelectTexture( 0 );
	glDisableVertexAttribArray( PC_ATTRIB_INDEX_ST );
	glEnable( GL_TEXTURE_GEN_S );
	glEnable( GL_TEXTURE_GEN_T );
	glEnable( GL_TEXTURE_GEN_Q );

	for ( i = 0 ; i < lightShader->GetNumStages() ; i++ ) {
		stage = lightShader->GetStage(i);

		if ( !regs[ stage->conditionRegister ] ) {
			continue;
		}

		GL_State( GLS_DEPTHMASK | stage->drawStateBits | GLS_DEPTHFUNC_EQUAL );

		GL_SelectTexture( 0 );
		stage->texture.image->Bind();

		if ( stage->texture.hasMatrix ) {
			RB_LoadShaderTextureMatrix( regs, &stage->texture );
		}

		// get the modulate values from the light, including alpha, unlike normal lights
		backEnd.lightColor[0] = regs[ stage->color.registers[0] ];
		backEnd.lightColor[1] = regs[ stage->color.registers[1] ];
		backEnd.lightColor[2] = regs[ stage->color.registers[2] ];
		backEnd.lightColor[3] = regs[ stage->color.registers[3] ];
		glColor4fv( backEnd.lightColor );

		RB_RenderDrawSurfChainWithFunction( drawSurfs, RB_T_BlendLight );
		RB_RenderDrawSurfChainWithFunction( drawSurfs2, RB_T_BlendLight );

		if ( stage->texture.hasMatrix ) {
			GL_SelectTexture( 0 );
			glMatrixMode( GL_TEXTURE );
			glLoadIdentity();
			glMatrixMode( GL_MODELVIEW );
		}
	}

	GL_SelectTexture( 1 );
	glDisable( GL_TEXTURE_GEN_S );
	globalImages->BindNull();

	GL_SelectTexture( 0 );
	glDisable( GL_TEXTURE_GEN_S );
	glDisable( GL_TEXTURE_GEN_T );
	glDisable( GL_TEXTURE_GEN_Q );
}


//========================================================================

static idPlane	fogPlanes[4];

/*
=====================
RB_T_BasicFog
=====================
*/
static void RB_T_BasicFog( const drawSurf_t *surf ) {
	if ( backEnd.currentSpace != surf->space ) {
		idPlane	local;

		GL_SelectTexture( 0 );

		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[0], local );
		local[3] += 0.5;
		glTexGenfv( GL_S, GL_OBJECT_PLANE, local.ToFloatPtr() );

//		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[1], local );
//		local[3] += 0.5;
        local[0] = local[1] = local[2] = 0; local[3] = 0.5;
		glTexGenfv( GL_T, GL_OBJECT_PLANE, local.ToFloatPtr() );

		GL_SelectTexture( 1 );

		// GL_S is constant per viewer
		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[2], local );
		local[3] += FOG_ENTER;
		glTexGenfv( GL_T, GL_OBJECT_PLANE, local.ToFloatPtr() );

		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[3], local );
		glTexGenfv( GL_S, GL_OBJECT_PLANE, local.ToFloatPtr() );
	}

	RB_T_RenderTriangleSurface( surf );
}



/*
==================
RB_FogPass
==================
*/
static void RB_FogPass( const drawSurf_t *drawSurfs,  const drawSurf_t *drawSurfs2 ) {
	const srfTriangles_t*frustumTris;
	drawSurf_t			ds;
	const idMaterial	*lightShader;
	const shaderStage_t	*stage;
	const float			*regs;

	RENDERLOG_PRINTF( "---------- RB_FogPass ----------\n" );

	// create a surface for the light frustom triangles, which are oriented drawn side out
	frustumTris = backEnd.vLight->frustumTris;

	// if we ran out of vertex cache memory, skip it
	if ( !frustumTris->ambientCache ) {
		return;
	}
	memset( &ds, 0, sizeof( ds ) );
	ds.space = &backEnd.viewDef->worldSpace;
	ds.geo = frustumTris;
	ds.scissorRect = backEnd.viewDef->scissor;

	// find the current color and density of the fog
	lightShader = backEnd.vLight->lightShader;
	regs = backEnd.vLight->shaderRegisters;
	// assume fog shaders have only a single stage
	stage = lightShader->GetStage(0);

	backEnd.lightColor[0] = regs[ stage->color.registers[0] ];
	backEnd.lightColor[1] = regs[ stage->color.registers[1] ];
	backEnd.lightColor[2] = regs[ stage->color.registers[2] ];
	backEnd.lightColor[3] = regs[ stage->color.registers[3] ];

	glColor3fv( backEnd.lightColor );

	// calculate the falloff planes
	float	a;

	// if they left the default value on, set a fog distance of 500
	if ( backEnd.lightColor[3] <= 1.0 ) {
		a = -0.5f / DEFAULT_FOG_DISTANCE;
	} else {
		// otherwise, distance = alpha color
		a = -0.5f / backEnd.lightColor[3];
	}

	GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );

	// texture 0 is the falloff image
	GL_SelectTexture( 0 );
	globalImages->fogImage->Bind();
	//GL_Bind( tr.whiteImage );
	glDisableVertexAttribArray( PC_ATTRIB_INDEX_ST );
	glEnable( GL_TEXTURE_GEN_S );
	glEnable( GL_TEXTURE_GEN_T );
	glTexCoord2f( 0.5f, 0.5f );		// make sure Q is set

	fogPlanes[0][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[2];
	fogPlanes[0][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[6];
	fogPlanes[0][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[10];
	fogPlanes[0][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[14];

	fogPlanes[1][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[0];
	fogPlanes[1][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[4];
	fogPlanes[1][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[8];
	fogPlanes[1][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[12];


	// texture 1 is the entering plane fade correction
	GL_SelectTexture( 1 );
	globalImages->fogEnterImage->Bind();
	glDisableVertexAttribArray( PC_ATTRIB_INDEX_ST );
	glEnable( GL_TEXTURE_GEN_S );
	glEnable( GL_TEXTURE_GEN_T );

	// T will get a texgen for the fade plane, which is always the "top" plane on unrotated lights
	fogPlanes[2][0] = 0.001f * backEnd.vLight->fogPlane[0];
	fogPlanes[2][1] = 0.001f * backEnd.vLight->fogPlane[1];
	fogPlanes[2][2] = 0.001f * backEnd.vLight->fogPlane[2];
	fogPlanes[2][3] = 0.001f * backEnd.vLight->fogPlane[3];

	// S is based on the view origin
	float s = backEnd.viewDef->renderView.vieworg * fogPlanes[2].Normal() + fogPlanes[2][3];

	fogPlanes[3][0] = 0;
	fogPlanes[3][1] = 0;
	fogPlanes[3][2] = 0;
	fogPlanes[3][3] = FOG_ENTER + s;

	glTexCoord2f( FOG_ENTER + s, FOG_ENTER );

	// draw it
	RB_RenderDrawSurfChainWithFunction( drawSurfs, RB_T_BasicFog );
	RB_RenderDrawSurfChainWithFunction( drawSurfs2, RB_T_BasicFog );

	// the light frustum bounding planes aren't in the depth buffer, so use depthfunc_less instead
	// of depthfunc_equal
	GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_LESS );
	GL_Cull( CT_BACK_SIDED );
	RB_RenderDrawSurfChainWithFunction( &ds, RB_T_BasicFog );
	GL_Cull( CT_FRONT_SIDED );

	GL_SelectTexture( 1 );
	glDisable( GL_TEXTURE_GEN_S );
	glDisable( GL_TEXTURE_GEN_T );
	globalImages->BindNull();

	GL_SelectTexture( 0 );
	glDisable( GL_TEXTURE_GEN_S );
	glDisable( GL_TEXTURE_GEN_T );
}


/*
==================
RB_STD_FogAllLights
==================
*/
void RB_STD_FogAllLights( void ) {
	viewLight_t	*vLight;

	if ( r_skipFogLights.GetBool() || r_showOverDraw.GetInteger() != 0 || backEnd.viewDef->isXraySubview ) {
		return;
	}

	RENDERLOG_PRINTF( "---------- RB_STD_FogAllLights ----------\n" );

	glDisable( GL_STENCIL_TEST );

	for ( vLight = backEnd.viewDef->viewLights ; vLight ; vLight = vLight->next ) {
		backEnd.vLight = vLight;

		if ( !vLight->lightShader->IsFogLight() && !vLight->lightShader->IsBlendLight() ) {
			continue;
		}

		if ( vLight->lightShader->IsFogLight() ) {
			RB_FogPass( vLight->globalInteractions, vLight->localInteractions );
		} else if ( vLight->lightShader->IsBlendLight() ) {
			RB_BlendLight( vLight->globalInteractions, vLight->localInteractions );
		}
	}

	glEnable( GL_STENCIL_TEST );
}


/*
=========================================================================================

GENERAL INTERACTION RENDERING

=========================================================================================
*/


/*
====================
GL_SelectTextureNoClient
====================
*/
static void GL_SelectTextureNoClient(int unit) {
	backEnd.glState.currenttmu = unit;
	glActiveTextureARB(GL_TEXTURE0_ARB + unit);
	RENDERLOG_PRINTF("glActiveTextureARB( %i )\n", unit);
}

/*
==================
RB_STD_DrawInteraction
==================
*/
void RB_STD_DrawInteraction(const drawInteraction_t *din) {
	// load all the vertex program parameters
	renderProgManager.SetRenderParm( RENDERPARM_LOCALLIGHTORIGIN, din->localLightOrigin.ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_LOCALVIEWORIGIN, din->localViewOrigin.ToFloatPtr() );

	renderProgManager.SetRenderParm( RENDERPARM_LIGHTPROJECTION_S, din->lightProjection[0].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_LIGHTPROJECTION_T, din->lightProjection[1].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_LIGHTPROJECTION_Q, din->lightProjection[2].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_LIGHTFALLOFF_S, din->lightProjection[3].ToFloatPtr() );

	renderProgManager.SetRenderParm( RENDERPARM_BUMPMATRIX_S, din->bumpMatrix[0].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_BUMPMATRIX_T, din->bumpMatrix[1].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_DIFFUSEMATRIX_S, din->diffuseMatrix[0].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_DIFFUSEMATRIX_T, din->diffuseMatrix[1].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_SPECULARMATRIX_S, din->specularMatrix[0].ToFloatPtr() );
	renderProgManager.SetRenderParm( RENDERPARM_SPECULARMATRIX_T, din->specularMatrix[1].ToFloatPtr() );

	// set the vertex colors
	RB_SetVertexColorParms( din->vertexColor );

	// set the textures

	// texture 0 will be the per-surface bump map
	GL_SelectTextureNoClient(0);
	din->bumpImage->Bind();

	// texture 1 will be the light falloff texture
	GL_SelectTextureNoClient(1);
	din->lightFalloffImage->Bind();

	// texture 2 will be the light projection texture
	GL_SelectTextureNoClient(2);
	din->lightImage->Bind();

	// texture 3 is the per-surface diffuse map
	GL_SelectTextureNoClient(3);
	din->diffuseImage->Bind();

	// texture 4 is the per-surface specular map
	GL_SelectTextureNoClient(4);
	din->specularImage->Bind();

	// draw it
	RB_DrawElementsWithCounters(din->surf->geo);
}


/*
=============
RB_STD_CreateDrawInteractions
=============
*/
void RB_STD_CreateDrawInteractions(const drawSurf_t *surf) {
	if (!surf) {
		return;
	}

	// perform setup here that will be constant for all interactions
	GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc);

	// select the render prog
	if ( surf->material->IsAmbientLight() ) {
		renderProgManager.BindShader_InteractionAmbient();
	} else {
		renderProgManager.BindShader_Interaction();
	}

	// enable the vertex arrays
	glEnableVertexAttribArray(PC_ATTRIB_INDEX_ST);
	glEnableVertexAttribArray(PC_ATTRIB_INDEX_TANGENT);
	glEnableVertexAttribArray(PC_ATTRIB_INDEX_BITANGENT);
	glEnableVertexAttribArray(PC_ATTRIB_INDEX_NORMAL);
	glEnableVertexAttribArray(PC_ATTRIB_INDEX_COLOR);

	for (; surf; surf = surf->nextOnLight) {
		// perform setup here that will not change over multiple interaction passes

		// set the vertex pointers
		idDrawVert	*ac = (idDrawVert *)vertexCache.Position(surf->geo->ambientCache);
        glVertexAttribPointer(PC_ATTRIB_INDEX_COLOR, 4, GL_UNSIGNED_BYTE, false, sizeof( idDrawVert ), (void *)&ac->color);
		glVertexAttribPointer(PC_ATTRIB_INDEX_NORMAL, 3, GL_FLOAT, false, sizeof(idDrawVert), ac->normal.ToFloatPtr());
		glVertexAttribPointer(PC_ATTRIB_INDEX_BITANGENT, 3, GL_FLOAT, false, sizeof(idDrawVert), ac->tangents[1].ToFloatPtr());
		glVertexAttribPointer(PC_ATTRIB_INDEX_TANGENT, 3, GL_FLOAT, false, sizeof(idDrawVert), ac->tangents[0].ToFloatPtr());
		glVertexAttribPointer(PC_ATTRIB_INDEX_ST, 2, GL_FLOAT, false, sizeof(idDrawVert), ac->st.ToFloatPtr());
        glVertexAttribPointer(PC_ATTRIB_INDEX_VERTEX, 3, GL_FLOAT, false, sizeof(idDrawVert), ac->xyz.ToFloatPtr());

		// this may cause RB_STD_DrawInteraction to be executed multiple
		// times with different colors and images if the surface or light have multiple layers
		RB_CreateSingleDrawInteractions(surf, RB_STD_DrawInteraction);
	}

	glDisableVertexAttribArray(PC_ATTRIB_INDEX_ST);
	glDisableVertexAttribArray(PC_ATTRIB_INDEX_TANGENT);
	glDisableVertexAttribArray(PC_ATTRIB_INDEX_BITANGENT);
	glDisableVertexAttribArray(PC_ATTRIB_INDEX_NORMAL);
    glDisableVertexAttribArray(PC_ATTRIB_INDEX_COLOR);
    
	// disable features
	GL_SelectTextureNoClient(4);
	globalImages->BindNull();

	GL_SelectTextureNoClient(3);
	globalImages->BindNull();

	GL_SelectTextureNoClient(2);
	globalImages->BindNull();

	GL_SelectTextureNoClient(1);
	globalImages->BindNull();

	backEnd.glState.currenttmu = -1;
	GL_SelectTexture(0);

	renderProgManager.Unbind();
}


/*
==================
RB_STD_DrawInteractions
==================
*/
void RB_STD_DrawInteractions(void) {
	viewLight_t		*vLight;
	const idMaterial	*lightShader;
    
    if ( r_skipInteractions.GetBool() ) {
        return;
    }

	renderLog.OpenMainBlock( MRB_DRAW_INTERACTIONS );
	renderLog.OpenBlock( "RB_DrawInteractions" );

	GL_SelectTexture(0);
	glDisableVertexAttribArray( PC_ATTRIB_INDEX_ST );

	//
	// for each light, perform adding and shadowing
	//
	for (vLight = backEnd.viewDef->viewLights; vLight; vLight = vLight->next) {
		backEnd.vLight = vLight;

		// do fogging later
		if (vLight->lightShader->IsFogLight()) {
			continue;
		}
		if (vLight->lightShader->IsBlendLight()) {
			continue;
		}

		if (!vLight->localInteractions && !vLight->globalInteractions && !vLight->translucentInteractions) {
			continue;
		}

		lightShader = vLight->lightShader;

		// clear the stencil buffer if needed
		if (vLight->globalShadows || vLight->localShadows) {
			backEnd.currentScissor = vLight->scissorRect;
			if (r_useScissor.GetBool()) {
				glScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
					backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
					backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
					backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
			}
			glClear(GL_STENCIL_BUFFER_BIT);
		} else {
			// no shadows, so no need to read or write the stencil buffer
			// we might in theory want to use GL_ALWAYS instead of disabling
			// completely, to satisfy the invarience rules
			glStencilFunc(GL_ALWAYS, 128, 255);
		}

		if ( vLight->globalShadows != NULL ) {
			renderLog.OpenBlock( "Global Light Shadows" );
			RB_StencilShadowPass( vLight->globalShadows );
			renderLog.CloseBlock();
		}

		if ( vLight->localInteractions != NULL ) {
			renderLog.OpenBlock( "Local Light Interactions" );
			RB_STD_CreateDrawInteractions( vLight->localInteractions );
			renderLog.CloseBlock();
		}

		if ( vLight->localShadows != NULL ) {
			renderLog.OpenBlock( "Local Light Shadows" );
			RB_StencilShadowPass( vLight->localShadows );
			renderLog.CloseBlock();
		}

		if ( vLight->globalInteractions != NULL ) {
			renderLog.OpenBlock( "Global Light Interactions" );
			RB_STD_CreateDrawInteractions( vLight->globalInteractions );
			renderLog.CloseBlock();
		}
		
		if ( vLight->translucentInteractions != NULL && !r_skipTranslucent.GetBool() ) {
			renderLog.OpenBlock( "Translucent Interactions" );

			// disable stencil shadow test
			glStencilFunc( GL_ALWAYS, 128, 255 );

			// Disable the depth bounds test because translucent surfaces don't work with
			// the depth bounds tests since they did not write depth during the depth pass.
			backEnd.depthFunc = GLS_DEPTHFUNC_LESS;

			// The depth buffer wasn't filled in for translucent surfaces, so they
			// can never be constrained to perforated surfaces with the depthfunc equal.

			// Translucent surfaces do not receive shadows. This is a case where a
			// shadow buffer solution would work but stencil shadows do not because
			// stencil shadows only affect surfaces that contribute to the view depth
			// buffer and translucent surfaces do not contribute to the view depth buffer.
			RB_STD_CreateDrawInteractions( vLight->translucentInteractions );

			backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;

			renderLog.CloseBlock();
		}
	}

	// disable stencil shadow test
	glStencilFunc(GL_ALWAYS, 128, 255);

	GL_SelectTexture(0);
	glEnableVertexAttribArray( PC_ATTRIB_INDEX_ST );

	renderLog.CloseBlock();
	renderLog.CloseMainBlock();
}

//=========================================================================================

/*
=============
RB_STD_DrawView
=============
*/
void	RB_STD_DrawView( void ) {
	drawSurf_t	 **drawSurfs;
	int			numDrawSurfs;

	RENDERLOG_PRINTF( "---------- RB_STD_DrawView ----------\n" );

	backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;

	drawSurfs = (drawSurf_t **)&backEnd.viewDef->drawSurfs[0];
	numDrawSurfs = backEnd.viewDef->numDrawSurfs;

	// clear the z buffer, set the projection matrix, etc
	RB_BeginDrawingView();
    
    // set shader program environment
    RB_SetProgramEnvironment();

	// fill the depth buffer and clear color buffer to black except on subviews
	RB_STD_FillDepthBuffer( drawSurfs, numDrawSurfs );

	// main light renderer
	RB_STD_DrawInteractions();

	// now draw any non-light dependent shading passes
	int	processed = RB_STD_DrawShaderPasses( drawSurfs, numDrawSurfs );

	// fob and blend lights
	RB_STD_FogAllLights();

	// now draw any post-processing effects using _currentRender
	if ( processed < numDrawSurfs ) {
		RB_STD_DrawShaderPasses( drawSurfs+processed, numDrawSurfs-processed );
	}

    // draw debug utilities
	RB_RenderDebugTools( drawSurfs, numDrawSurfs );
}
