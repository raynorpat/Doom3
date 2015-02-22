#version 120      
 
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
  
uniform mat4				u_modelMatrix; 
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