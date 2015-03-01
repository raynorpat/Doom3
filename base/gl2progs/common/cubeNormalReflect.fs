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

varying vec2            	var_TexCoord;
varying vec4            	var_EyeVector;
varying vec4            	var_Color;

uniform samplerCube			u_envMap;
uniform sampler2D			u_normalMap;

void main( void ) {
    vec3 globalEye = normalize( var_EyeVector ).xyz;
    
    vec3 localNormal = texture2D( u_normalMap, var_TexCoord ).rgb * 2.0 - 1.0;
    localNormal = normalize( localNormal );
    
    // modulate the environment map by the vertex color
    gl_FragColor = textureCube( u_envMap, reflect( -globalEye, localNormal ) ) * var_Color;
}