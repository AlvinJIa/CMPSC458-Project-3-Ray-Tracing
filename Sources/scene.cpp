#include "scene.h"

scene::scene(const char* filename)
{
	parse(filename);
	std::cout << std::endl << "background: " << glm::to_string(bgColor);
	std::cout << std::endl << "ambient: " << glm::to_string(ambLight);
	std::cout << std::endl << "eye: " << glm::to_string(eye);
	std::cout << std::endl << "lookAt: " << glm::to_string(lookAt);
	std::cout << std::endl << "up: " << glm::to_string(up);
	std::cout << std::endl << "fovy: " << fovy;
	std::cout << std::endl;
}

glm::vec3 scene::rayTrace(glm::vec3 eye, glm::vec3 dir, int recurseDepth)
{
	//start with black, add color as we go
	glm::vec3 answer(0.0f);

	//test for intersection against all our objects
	float dist = myObjGroup->testIntersections(eye, dir);
	//if we saw nothing, return the background color of our scene
	if (dist == 9999999)
		return bgColor;

	//get the material index and normal vector(at the point we saw) of the object we saw
	int matIndex = myObjGroup->getClosest()->getMatIndex();
	rtObject *curr = myObjGroup->getClosest();
	material * texture = &myMaterials.at(matIndex);
	glm::vec3 normal = myObjGroup->getClosest()->getNormal(eye, dir);

	//determine texture color
	glm::vec3 textureColor;

	if (!texture->image)
		//this is multiplicative, rather than additive
		//so if there is no texture, just use ones
		textureColor = glm::vec3(1.0f);
	else
	{
		//if there is a texture image, ask the object for the image coordinates (between 0 and 1)
		glm::vec2 coords = myObjGroup->getClosest()->getTextureCoords(eye, dir);

		//get the color from that image location
		
		int x = (int)(texture->width*coords.x);
		int y = (int)(texture->height*coords.y);
		if (y == texture->height) y--;
		if (x == texture->width) x--;
		textureColor = texture->data[x + y*texture->width];
	}

	//add diffuse color times ambient light to our answer
	answer += (texture->diffuseCol) * ambLight;
	//iterate through all lights
	glm::vec3 point = eye + (dist * dir);
	glm::vec3 lightdir;
	float d, shadowdist;
	//if the light can see the surface point,
	//add its diffuse color to a total diffuse for the point (using our illumination model)
	//use testIntersection to help decide this
	for (int i = 0; i < myLights.size(); i++)
	{	
		lightdir = glm::normalize(myLights[i].position - point);
		d = myObjGroup->testIntersections(myLights[i].position, lightdir);
		shadowdist = myObjGroup->testIntersections(point + 0.001f, lightdir);
		rtObject * next = myObjGroup->getClosest();
		if (abs(d) == 9999999 && curr==next) {

			answer += texture->diffuseCol*(myLights[i].color * fmax(glm::dot(normal, lightdir), 0.0f));

			glm::vec3 h = glm::normalize(lightdir - dir);
			answer += myLights[i].color * texture->specularCol * glm::pow(glm::dot(normal, h), texture->shininess);
		}
		//if (abs(d) == 9999999 && abs(shadowdist) != 9999999 && curr != next)
		//	answer *= 0.0f;
	}
	//rtObject * next = myObjGroup->getClosest();
	//if (abs(d) == 9999999 && curr != next)
	//	answer *= 0.0f;

	//put a limit on the depth of recursion
	if (recurseDepth < 3) {
		if (texture->reflectiveCol != glm::vec3(0)) {
			glm::vec3 flectR = glm::normalize(dir - (2 * (glm::dot(dir, normal)) * normal));
			answer += (texture->reflectiveCol * rayTrace(point, flectR, recurseDepth + 1));
		}
		glm::vec3 fractR;
		glm::vec3 fractC;
		//into material
		if (glm::dot(normal, dir) < 0) {
			float theta1 = acos(glm::dot(normal,-dir));
			float phi1 = sin(theta1) * texture->refractionIndex;
			fractR = glm::normalize(((dir + normal * cos(theta1)) * (texture->refractionIndex)) - (normal * cos(phi1)));
			fractC = rayTrace(point, fractR, recurseDepth + 1);
		}
		//bend away
		else {
			float theta2 = acos(glm::dot(-normal, -dir));
			float phi2 = sin(theta2) * texture->refractionIndex;
			fractR = glm::normalize(((dir + (-normal * cos(theta2))) / (texture->refractionIndex)) - (-normal * cos(phi2)));
			fractC = rayTrace(point, fractR, recurseDepth + 1);
		}
		answer += texture->transparentCol * fractC;
	}
	answer *= textureColor;
	if (answer.x > 1.0f) answer.x = 1.0f;
	if (answer.y > 1.0f) answer.y = 1.0f;
	if (answer.z > 1.0f) answer.z = 1.0f;
	return answer;
}

glm::vec3 scene::getEye()
{
	return eye;
}

glm::vec3 scene::getLookAt()
{
	return lookAt;
}

glm::vec3 scene::getUp()
{
	return up;
}

float scene::getFovy()
{
	return fovy;
}

void scene::parse(const char *filename)
{
	//some default values in case parsing fails
	myObjGroup = NULL;
	bgColor = glm::vec3(0.5, 0.5, 0.5);
	ambLight = glm::vec3(0.5, 0.5, 0.5);
	eye = glm::vec3(0, 0, 0);
	lookAt = glm::vec3(0, 0, -1);
	up = glm::vec3(0, 1, 0);
	fovy = 45;
	file = NULL;
	curline = 1;

	//the file-extension needs to be "ray"
	assert(filename != NULL);
	const char *ext = &filename[strlen(filename) - 4];
	// if no ray extention
	if (strcmp(ext, ".ray"))
	{
		printf("ERROR::SCENE::FILE_NOT_SUCCESFULLY_READ::\"%s\"\n", filename);
		assert(!strcmp(ext, ".ray"));
	}
	file = fopen(filename, "r");
	// Fails if no file exists
	if (file == NULL)
	{
		printf("ERROR::SCENE::FILE_NOT_SUCCESFULLY_READ::\"%s\"\n", filename);
		//assert(file != NULL);
	}
	char token[MAX_PARSER_TOKEN_LENGTH];

	//prime the parser pipe
	parse_char = fgetc(file);

	while (getToken(token))
	{
		if (!strcmp(token, "Background"))
			parseBackground();
		else if (!strcmp(token, "Camera"))
			parseCamera();
		else if (!strcmp(token, "Materials"))
			parseMaterials();
		else if (!strcmp(token, "Group"))
			myObjGroup = parseGroup();
		else if (!strcmp(token, "Lights"))
			parseLights();
		else
		{
			std::cout << "Unknown token in parseFile: '" << token <<
				"' at input line " << curline << "\n";
			exit(-1);
		}
	}
}

/* Parse the "Camera" token */
void scene::parseCamera()
{
	char token[MAX_PARSER_TOKEN_LENGTH];
	getToken(token); assert(!strcmp(token, "{"));

	//get the eye, center, and up vectors (similar to gluLookAt)
	getToken(token); assert(!strcmp(token, "eye"));
	eye = readVec3f();

	getToken(token); assert(!strcmp(token, "lookAt"));
	lookAt = readVec3f();

	getToken(token); assert(!strcmp(token, "up"));
	up = readVec3f();


	getToken(token); assert(!strcmp(token, "fovy"));
	fovy = readFloat();

	getToken(token); assert(!strcmp(token, "}"));
}

/* Parses the "Background" token */
void scene::parseBackground()
{
	char token[MAX_PARSER_TOKEN_LENGTH];

	getToken(token); assert(!strcmp(token, "{"));
	while (1)
	{
		getToken(token);
		if (!strcmp(token, "}"))
		{
			break;
		}
		else if (!strcmp(token, "color"))
		{
			bgColor = readVec3f();
		}
		else if (!strcmp(token, "ambientLight"))
		{
			ambLight = readVec3f();
		}
		else
		{
			std::cout << "Unknown token in parseBackground: " << token << "\n";
			assert(0);
		}
	}
}

/* Parses the "Group" token */
rtObjGroup* scene::parseGroup()
{
	char token[MAX_PARSER_TOKEN_LENGTH];


	getToken(token);
	assert(!strcmp(token, "{"));

	/**********************************************/
	/* Instantiate the group object               */
	/**********************************************/
	rtObjGroup *answer = new rtObjGroup();

	bool working = true;
	while (working)
	{
		getToken(token);
		if (!strcmp(token, "}"))
		{
			working = false;
		}
		else
		{
			if (!strcmp(token, "Sphere"))
			{
				sphere *sceneElem = parseSphere();
				assert(sceneElem != NULL);
				answer->addObj(sceneElem);
			}
			else if (!strcmp(token, "Triangle"))
			{
				triangle *sceneElem = parseTriangle();
				assert(sceneElem != NULL);
				answer->addObj(sceneElem);
			}
			else
			{
				std::cout << "Unknown token in Group: '" << token << "' at line "
					<< curline << "\n";
				exit(0);
			}
		}
	}

	/* Return the group */
	return answer;
}

/* Parse the "Sphere" token */
sphere* scene::parseSphere()
{
	char token[MAX_PARSER_TOKEN_LENGTH];

	getToken(token); assert(!strcmp(token, "{"));
	getToken(token); assert(!strcmp(token, "materialIndex"));
	int sphere_material_index = readInt();

	getToken(token); assert(!strcmp(token, "center"));
	glm::vec3 center = readVec3f();
	getToken(token); assert(!strcmp(token, "radius"));
	float radius = readFloat();
	getToken(token); assert(!strcmp(token, "}"));

	std::printf("Sphere:\n\tCenter: %s\n", glm::to_string(center).c_str());
	std::printf("\tRadius: %f\n", radius);
	std::printf("\tSphere Material Index: %d\n\n", sphere_material_index);

	/**********************************************/
	/* The call to your own constructor goes here */
	/**********************************************/
	return new sphere(center, radius, sphere_material_index, this);
}

/* Parse out the "Triangle" token */
triangle* scene::parseTriangle()
{
	char token[MAX_PARSER_TOKEN_LENGTH];
	getToken(token); assert(!strcmp(token, "{"));

	/* Parse out vertex information */
	getToken(token); assert(!strcmp(token, "vertex0"));
	glm::vec3 v0 = readVec3f();
	getToken(token); assert(!strcmp(token, "vertex1"));
	glm::vec3 v1 = readVec3f();
	getToken(token); assert(!strcmp(token, "vertex2"));
	glm::vec3 v2 = readVec3f();
	getToken(token); assert(!strcmp(token, "tex_xy_0"));
	float x0 = 0;
	float y0 = 0;
	x0 = readFloat();
	y0 = readFloat();
	getToken(token); assert(!strcmp(token, "tex_xy_1"));
	float x1 = 0;
	float y1 = 0;
	x1 = readFloat();
	y1 = readFloat();
	getToken(token); assert(!strcmp(token, "tex_xy_2"));
	float x2 = 0;
	float y2 = 0;
	x2 = readFloat();
	y2 = readFloat();
	getToken(token); assert(!strcmp(token, "materialIndex"));
	int mat = readInt();

	getToken(token); assert(!strcmp(token, "}"));


	std::printf("Triangle:\n");
	std::printf("\tVertex0: %s tex xy 0 %s\n", glm::to_string(v0).c_str(), glm::to_string(glm::vec2(x0, y0)).c_str());
	std::printf("\tVertex1: %s tex xy 1 %s\n", glm::to_string(v1).c_str(), glm::to_string(glm::vec2(x1, y1)).c_str());
	std::printf("\tVertex1: %s tex xy 1 %s\n", glm::to_string(v2).c_str(), glm::to_string(glm::vec2(x2, y2)).c_str());
	std::printf("\tTriangle Material Index: %d\n\n", mat);

	/**********************************************/
	/* The call to your own constructor goes here */
	/**********************************************/
	return new triangle(v0, v1, v2, x0, x1, x2, y0, y1, y2, mat, this);
}

/* Parse the "Materials" token */
void scene::parseMaterials()
{
	char token[MAX_PARSER_TOKEN_LENGTH];
	char texname[MAX_PARSER_TOKEN_LENGTH];
	getToken(token); assert(!strcmp(token, "{"));

	/* Loop over each Material */
	bool working = true;
	while (working)
	{
		getToken(token);
		if (!strcmp(token, "}"))
		{
			working = false;
		}
		else if (!strcmp(token, "Material"))
		{
			getToken(token); assert(!strcmp(token, "{"));
			texname[0] = '\0';
			glm::vec3 diffuseColor(1, 1, 1);
			glm::vec3 specularColor(0, 0, 0);
			float shininess = 1;
			glm::vec3 transparentColor(0, 0, 0);
			glm::vec3 reflectiveColor(0, 0, 0);
			float indexOfRefraction = 1;

			while (1)
			{
				getToken(token);
				if (!strcmp(token, "textureFilename"))
				{
					getToken(token);
					strcpy(texname, token);
				}
				else if (!strcmp(token, "diffuseColor"))
					diffuseColor = readVec3f();
				else if (!strcmp(token, "specularColor"))
					specularColor = readVec3f();
				else if (!strcmp(token, "shininess"))
					shininess = readFloat();
				else if (!strcmp(token, "transparentColor"))
					transparentColor = readVec3f();
				else if (!strcmp(token, "reflectiveColor"))
					reflectiveColor = readVec3f();
				else if (!strcmp(token, "indexOfRefraction"))
					indexOfRefraction = readFloat();
				else
				{
					assert(!strcmp(token, "}"));
					break;
				}
			}

			material temp;

			std::printf("Material:\n", texname);
			temp.diffuseCol = diffuseColor;
			std::printf("\tDiffuse Color: %s\n", glm::to_string(diffuseColor).c_str());
			temp.specularCol = specularColor;
			std::printf("\tSpecular Color: %s\n", glm::to_string(specularColor).c_str());
			temp.shininess = shininess;
			std::printf("\tShininess: %f\n", shininess);
			temp.transparentCol = transparentColor;
			std::printf("\tTransparent Color: %s\n", glm::to_string(transparentColor).c_str());
			temp.reflectiveCol = reflectiveColor;
			std::printf("\tReflective Color: %s\n", glm::to_string(reflectiveColor).c_str());
			temp.refractionIndex = indexOfRefraction;
			std::printf("\tIndex Of Refraction: %f\n", indexOfRefraction);
			std::printf("\tFileName: %s\n\n", texname);
			if (strcmp(texname, "NULL"))
			{
				temp.image = true;
				unsigned char *raw_data = stbi_load(texname, &temp.width, &temp.height, &temp.nrComponents, 0);
				if (raw_data)
				{
					// Make vector for each image
					for (int y = 0; y < temp.height; y++)
						for (int x = 0; x < temp.width; x++)
							if (temp.nrComponents == 1)
								temp.data.push_back(glm::vec3((float)raw_data[x + y*temp.height]));
							else
								temp.data.push_back(glm::vec3((float)raw_data[temp.nrComponents * (x + y*temp.width)],
									(float)raw_data[temp.nrComponents * (x + y*temp.width) + 1],
									(float)raw_data[temp.nrComponents * (x + y*temp.width) + 2]) / 255.0f);
					stbi_image_free(raw_data);
				}
				else
				{
					std::cout << "Texture failed to load at path: " << texname << std::endl;
					stbi_image_free(raw_data);
				}
			}
			else
				temp.image = false;

			myMaterials.push_back(temp);

		}
	}
}

void scene::parseLights()
{
	char token[MAX_PARSER_TOKEN_LENGTH];
	char texname[MAX_PARSER_TOKEN_LENGTH];
	getToken(token); assert(!strcmp(token, "{"));

	/* Loop over each Material */
	bool working = true;
	while (working)
	{
		getToken(token);
		if (!strcmp(token, "}"))
		{
			working = false;
		}
		else if (!strcmp(token, "Light"))
		{
			getToken(token); assert(!strcmp(token, "{"));
			texname[0] = '\0';
			glm::vec3 position(0, 0, 0);
			glm::vec3 color(1, 1, 1);

			while (1)
			{
				getToken(token);
				if (!strcmp(token, "position"))
					position = readVec3f();
				else if (!strcmp(token, "color"))
					color = readVec3f();
				else
				{
					assert(!strcmp(token, "}"));
					break;
				}
			}

			/**********************************************/
			/* The call to your own constructor goes here */
			/**********************************************/
			light temp;
			temp.position = position;
			std::printf("Light:\n\tPostion: %s\n", glm::to_string(position).c_str());
			temp.color = color;
			std::printf("\Color: %s\n\n", glm::to_string(color).c_str());
			myLights.push_back(temp);

		}
	}
}

/* consume whitespace */
void scene::eatWhitespace(void)
{
	bool working = true;

	do {
		while (isspace(parse_char))
		{
			if (parse_char == '\n')
			{
				curline++;
			}
			parse_char = fgetc(file);
		}

		if ('#' == parse_char)
		{
			/* this is a comment... eat until end of line */
			while (parse_char != '\n')
			{
				parse_char = fgetc(file);
			}

			curline++;
		}
		else
		{
			working = false;
		}

	} while (working);
}

/* Parse out a single token */
int scene::getToken(char token[MAX_PARSER_TOKEN_LENGTH])
{
	int idx = 0;

	assert(file != NULL);
	eatWhitespace();

	if (parse_char == EOF)
	{
		token[0] = '\0';
		return 0;
	}
	while ((!(isspace(parse_char))) && (parse_char != EOF))
	{
		token[idx] = parse_char;
		idx++;
		parse_char = fgetc(file);
	}

	token[idx] = '\0';
	return 1;
}

/* Reads in a 3-vector */
glm::vec3 scene::readVec3f()
{	
	float a = readFloat();
	float b = readFloat();
	float c = readFloat();
	return glm::vec3(a, b, c);
}

/* Reads in a single float */
float scene::readFloat()
{
	float answer;
	char buf[MAX_PARSER_TOKEN_LENGTH];

	if (!getToken(buf))
	{
		std::cout << "Error trying to read 1 float (EOF?)\n";
		assert(0);
	}

	int count = sscanf(buf, "%f", &answer);
	if (count != 1)
	{
		std::cout << "Error trying to read 1 float\n";
		assert(0);

	}
	return answer;
}

/* Reads in a single int */
int scene::readInt()
{
	int answer;
	char buf[MAX_PARSER_TOKEN_LENGTH];

	if (!getToken(buf))
	{
		std::cout << "Error trying to read 1 int (EOF?)\n";
		assert(0);
	}

	int count = sscanf(buf, "%d", &answer);
	if (count != 1)
	{
		std::cout << "Error trying to read 1 int\n";
		assert(0);

	}
	return answer;
}
