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
varying vec4            	var_TexLight;      
varying mat3     			var_TangentBinormalNormalMatrix;      
varying vec4            	var_Color;        
     
uniform sampler2D   		u_normalTexture;         
uniform sampler2D   		u_lightFalloffTexture;         
uniform sampler2D   		u_lightProjectionTexture;         
uniform sampler2D   		u_diffuseTexture;   
         
uniform vec4 				u_lightOrigin;   
uniform vec4 				u_diffuseColor;    
      
void main( void ) {         
	// compute normalized light direction
	vec3 L = normalize( u_lightOrigin.xyz - var_Position ); 

	// compute normal from normal map, move from [0, 1] to [-1, 1] range, normalize    
	vec3 N = normalize( ( 2.0 * texture2D ( u_normalTexture, var_TexNormal.st ).agb ) - 1.0 ); 
	N = var_TangentBinormalNormalMatrix * N;  
	 
	// compute the diffuse term     
	vec4 diffuse = texture2D( u_diffuseTexture, var_TexDiffuse );     
	diffuse *= u_diffuseColor; 
	diffuse *= max( dot( N, L ), 0.0);
    
	// compute light projection and falloff 
	vec3 lightFalloff		= texture2D( u_lightFalloffTexture, vec2( var_TexLight.z, 0.5 ) ).rgb;       
	vec3 lightProjection	= texture2DProj( u_lightProjectionTexture, var_TexLight.xyw ).rgb; 

	// compute lighting model     
    vec4 color = diffuse;
	color.rgb *= lightProjection; 
	color.rgb *= lightFalloff; 
	color.rgb *= var_Color.rgb;
 
	// output final color     
	gl_FragColor = color;        
}