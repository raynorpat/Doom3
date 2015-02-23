/*
 ===========================================================================
 
 Doom 3 GPL Source Code
 Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.
 Copyright (C) 2015 Pat 'raynorpat' Raynor <raynorpat@gmail.com>
 
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

varying vec3          		var_Position;
varying vec2            	var_TexDiffuse;  
varying vec2            	var_TexNormal;  
varying vec2            	var_TexSpecular;  
varying vec4            	var_TexLight;  
varying mat3				var_TangentBitangentNormalMatrix; 
varying vec4            	var_Color;  

attribute vec4          	attr_TexCoord;   
attribute vec3          	attr_Tangent;
attribute vec3          	attr_Bitangent;   
attribute vec3          	attr_Normal;   
  
uniform vec4 				u_lightProjectionS;  
uniform vec4 				u_lightProjectionT;  
uniform vec4 				u_lightFalloff;  
uniform vec4 				u_lightProjectionQ;   
uniform vec4 				u_bumpMatrixS;   
uniform vec4 				u_bumpMatrixT;   
uniform vec4 				u_diffuseMatrixS;   
uniform vec4 				u_diffuseMatrixT;   
uniform vec4 				u_specularMatrixS;   
uniform vec4 				u_specularMatrixT;   
uniform vec4 				u_colorModulate;  
uniform vec4				u_colorAdd;  
uniform vec4 				u_lightOrigin;        
uniform vec4 				u_viewOrigin;   
      
void main( void ) {
    // transform vertex position into homogenous clip-space   
	gl_Position = ftransform( );  
 
	// transform vertex position into world space   
	var_Position = gl_Vertex.xyz;

	// normal map texgen   
	var_TexNormal.x = dot( u_bumpMatrixS, attr_TexCoord );
	var_TexNormal.y = dot( u_bumpMatrixT, attr_TexCoord ); 
 
	// diffuse map texgen      
	var_TexDiffuse.x = dot( u_diffuseMatrixS, attr_TexCoord );
	var_TexDiffuse.y = dot( u_diffuseMatrixT, attr_TexCoord );
 
	// specular map texgen  
	var_TexSpecular.x = dot( u_specularMatrixS, attr_TexCoord );
	var_TexSpecular.y = dot( u_specularMatrixT, attr_TexCoord );
 
	// light projection texgen
	var_TexLight.x = dot( u_lightProjectionS, gl_Vertex );   
	var_TexLight.y = dot( u_lightProjectionT, gl_Vertex );   
	var_TexLight.z = dot( u_lightFalloff, gl_Vertex );   
	var_TexLight.w = dot( u_lightProjectionQ, gl_Vertex );   
	
	// construct tangent-bitangent-normal 3x3 matrix   
	var_TangentBitangentNormalMatrix = mat3( clamp(attr_Tangent,-1,1), clamp(attr_Bitangent,-1,1), clamp(attr_Normal,-1,1) );
 
	// primary color  
	var_Color = (gl_FrontColor * u_colorModulate) + u_colorAdd;   
}