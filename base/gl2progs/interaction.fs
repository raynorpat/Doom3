#version 120         
         
varying vec3     			var_Position; 
varying vec2            	var_TexDiffuse;        
varying vec2            	var_TexNormal;        
varying vec2            	var_TexSpecular;       
varying vec4            	var_TexLight; 
varying mat3				var_TangentBitangentNormalMatrix; 
varying vec4            	var_Color;        
     
uniform sampler2D   		u_normalTexture;         
uniform sampler2D   		u_lightFalloffTexture;         
uniform sampler2D   		u_lightProjectionTexture;         
uniform sampler2D   		u_diffuseTexture;         
uniform sampler2D   		u_specularTexture;        
         
uniform vec4 				u_lightOrigin;        
uniform vec4 				u_viewOrigin;        
uniform vec4 				u_diffuseColor;        
uniform vec4 				u_specularColor;        

void main( void ) {
    // compute light and view direction
	vec3 lightDir	= u_lightOrigin.xyz - var_Position;
	vec3 viewDir	= u_viewOrigin.xyz - var_Position;

	// compute normalized light, view and half angle vectors 
	vec3 L = normalize( lightDir ); 
	vec3 V = normalize( viewDir ); 
	vec3 H = normalize( L + V );
 
	// compute normal from normal map, move from [0, 1] to [-1, 1] range, normalize 
	vec3 N = normalize( ( 2.0 * texture2D ( u_normalTexture, var_TexNormal.st ).wyz ) - 1.0 ); 
	N = var_TangentBitangentNormalMatrix * N; 
	
	// compute the diffuse term    
	vec3 diffuse = texture2D( u_diffuseTexture, var_TexDiffuse ).rgb * u_diffuseColor.rgb;

	// compute the specular term
    float specularPower = 10.0;
    float NdotH = clamp( dot( N, H ), 0.0, 1.0 );
    float specularContribution = pow( NdotH, specularPower );
	vec3 specular = texture2D( u_specularTexture, var_TexSpecular ).rgb * specularContribution * u_specularColor.rgb;

	// compute light projection and falloff 
	vec3 lightFalloff		= texture2D( u_lightFalloffTexture, vec2( var_TexLight.z, 0.5 ) ).rgb;
	vec3 lightProjection	= texture2DProj( u_lightProjectionTexture, var_TexLight.xyw ).rgb; 

	// compute lighting model
    float NdotL = dot( N, L );
	vec3 lightColor = lightProjection * lightFalloff;
    vec3 finalColor = ( diffuse + specular ) * NdotL * lightColor * var_Color.rgb;
 
	// output final color     
	gl_FragColor.rgb = finalColor;
    gl_FragColor.a = 1.0;
}