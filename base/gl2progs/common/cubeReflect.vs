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

varying vec4            	var_NormalVector;
varying vec4            	var_EyeVector;
varying vec4            	var_Color;
  
uniform vec3				u_ViewOrigin;

void main( void ) {
	// mvp transform into clip space
	gl_Position = ftransform();

    // compute the normal vector
    var_NormalVector = vec4( gl_Normal * 2.0 - 1.0, 1.0 );
    
    // compute the eye vector
    var_EyeVector = vec4( u_ViewOrigin, 1.0 ) - gl_Position;

	// primary color
	var_Color = gl_Color;
}