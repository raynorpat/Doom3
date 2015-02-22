#version 120         

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