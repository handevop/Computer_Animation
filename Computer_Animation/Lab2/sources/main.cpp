// Local Headers
#include <FPSManager.h>
#include "Shader.h"

// System Headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <assimp/Importer.hpp>      
#include <assimp/scene.h>           
#include <assimp/postprocess.h> 
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Standard Headers
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <array>
#include <iostream>
#include "main.h"
#include <glm/gtc/type_ptr.hpp>

int width = 1200, height = 900;
std::vector< float > load_vrhovi, load_boje;
std::vector< unsigned int > load_indeksi;

//na temelju naziva sjencara izradi objekt sjencara. Npr. shader0 -> shader0.vert i shader0.frag
Shader* loadShader(char* path, char* naziv) {
	std::string sPath(path);
	std::string pathVert;
	std::string pathFrag;

	//malo je nespretno napravljeno jer ne koristimo biblioteku iz c++17, a treba podrzati i windows i linux

	pathVert.append(path, sPath.find_last_of("\\/") + 1);
	pathFrag.append(path, sPath.find_last_of("\\/") + 1);
	if (pathFrag[pathFrag.size() - 1] == '/') {
		pathVert.append("shaders/");
		pathFrag.append("shaders/");
	}
	else if (pathFrag[pathFrag.size() - 1] == '\\') {
		pathVert.append("shaders\\");
		pathFrag.append("shaders\\");
	}
	else {
		std::cerr << "nepoznat format pozicije shadera";
		exit(1);
	}

	pathVert.append(naziv);
	pathVert.append(".vert");
	pathFrag.append(naziv);
	pathFrag.append(".frag");

	return new Shader(pathVert.c_str(), pathFrag.c_str());
}


void framebuffer_size_callback(GLFWwindow* window, int Width, int Height)
{
	width = Width;
	height = Height;
	glViewport(0, 0, width, height);
}

bool load_object(std::string putanja, std::string objekt) {
	Assimp::Importer importer;

	std::string path(putanja);
	std::string dirPath(path, 0, path.find_last_of("\\/"));
	std::string resPath(dirPath);
	resPath.append("\\resources"); //za linux pretvoriti u forwardslash
	std::string objPath(resPath);
	objPath.append("\\glava\\"); //za linux pretvoriti u forwardslash
	objPath.append(objekt);

	const aiScene* scene = importer.ReadFile(objPath.c_str(),
		aiProcess_CalcTangentSpace |
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_SortByPType |
		aiProcess_FlipUVs
		//aiProcess_GenNormals
	);

	if (!scene) {
		std::cerr << importer.GetErrorString();
		return false;
	}

	if (scene->HasMeshes()) {

		aiMesh* mesh = scene->mMeshes[0];

		//std::cout << "ucitana poligonalna mreza" << std::endl;

		//std::cout << "prvih 10 tocaka:" << std::endl;
		//popis svih tocaka u modelu s x, y, z koordinatama
		for (int i = 0; i < mesh->mNumVertices; i++) {
			//std::cout << mesh->mVertices[i].x << " " << mesh->mVertices[i].y << " " << mesh->mVertices[i].z << std::endl;
			load_vrhovi.push_back(mesh->mVertices[i].x);
			load_vrhovi.push_back(mesh->mVertices[i].y);
			load_vrhovi.push_back(mesh->mVertices[i].z);
			load_vrhovi.push_back(1); //homohena koordinata
		}
		//std::cout << std::endl;

		//std::cout << "prvih 10 poligona:" << std::endl;

		//svaki poligon se sastoji od 3 ili vise tocki. 
		for (int i = 0; i < mesh->mNumFaces; i++) {

			for (int j = 0; j < mesh->mFaces[i].mNumIndices; j++) {
				//std::cout << mesh->mFaces[i].mIndices[j] << " ";

				load_indeksi.push_back(mesh->mFaces[i].mIndices[j]);
			}
			//std::cout << "-> " << std::endl;
			//Do koordinata tih tocaka se dolazi preko prijasnjeg popisa.
			//OPREZ! razmisliti prije nego se kopiraju dijelovi ovog koda. OpenGL želi indeksirane vrhove, normale i uv koordinate, a tu smo prikazali te podatke slijedno, a ne indeksirano!

			for (int j = 0; j < mesh->mFaces[i].mNumIndices; j++) {
				int vertex = mesh->mFaces[i].mIndices[j]; //OPREZ! grafickoj kartici cete po uputama slati indeksirane buffere, a ne slijedne
				//std::cout << "   coordinates    xyz  " << mesh->mVertices[vertex].x << " " << mesh->mVertices[vertex].y << " " << mesh->mVertices[vertex].z << std::endl;
			}
			//std::cout << std::endl;

		}


	}

	return true;
}

GLuint loadTextureRGBA(const std::string& exePath, const std::string& filenameInResources)
{
	std::string path(exePath);
	size_t slash = path.find_last_of("\\/");
	std::string dir = (slash == std::string::npos) ? "." : path.substr(0, slash);

	// ako ti je /resources/iskrica.png:
	std::string fullPath = dir + "/resources/" + filenameInResources;

	stbi_set_flip_vertically_on_load(true);

	int w = 0, h = 0, n = 0;
	unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 4); // prisili RGBA
	if (!data) {
		std::cerr << "Texture load failed: " << fullPath << "\n";
		std::cerr << "stbi: " << stbi_failure_reason() << "\n";
		return 0;
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	stbi_image_free(data);
	glBindTexture(GL_TEXTURE_2D, 0);

	return tex;
}


class Transform {
public:
	std::vector< float > vrhovi;
	std::vector< float > boje;
	std::vector< unsigned int > indeksi;
	glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);  // gdje se objekt nalazi
	float yaw = 0.0f;    // rotacija oko Y osi
	float pitch = 0.0f;  // rotacija oko X osi
	float roll = 0.0f;   // rotacija oko Z osi
	glm::mat4 model = glm::mat4(1.0f);
	glm::vec3 camPosition = glm::vec3(-12.0f, 3.0f, 12.0f); // Daleko lijevo i malo nazad
	glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 camFront = glm::vec3(2.2f, -0.3f, -0.9f); // Snažno gledanje desno
	glm::mat4 view = glm::lookAt(camPosition,
		camPosition + camFront,
		worldUp);
	glm::mat4 projection = glm::mat4(1.0f);

	glm::mat4 getViewMatrix() {
		return view;
	}

	glm::mat4 getModelMatrix() {
		return model;
	}
};

class Mesh : public Transform {
public:
	std::vector< float > vrhovi;
	std::vector< float > boje;
	std::vector< unsigned int > indeksi;
	glm::vec3 boja;
	glm::mat4 transform;

	int counter = 0;

	Mesh() {
		boja.r = 1;
		boja.g = 0;
		boja.b = 0;

		transform = glm::mat4(1);
	}

	glm::vec3 changeColor(glm::vec3 color, int counter) {
		float increment = 0.5;
		float newR = 0;
		float newG = 0;
		float newB = 0;

		if (counter < 3)
			newR = 1;
		else if (counter < 6)
			newG = 1;
		else
			newB = 1;

		color.r = newR;
		color.g = newG;
		color.b = newB;

		return color;
	}

	void add_vrh(glm::vec3 vrh) {
		vrhovi.push_back((float)vrh.x);
		vrhovi.push_back((float)vrh.y);
		vrhovi.push_back((float)vrh.z);
		vrhovi.push_back(1.0); // homogena koordinata

		boje.push_back((float)(boja.r));
		boje.push_back((float)boja.g);
		boje.push_back((float)boja.b);


		boja = changeColor(boja, counter);

		counter = (counter + 1) % 9;
	}

	std::vector< float > get_vrhovi() {
		return vrhovi;
	}

	std::vector< float > get_boje() {
		return boje;
	}

	std::vector< unsigned int > get_indeksi() {
		return indeksi;
	}

	void add_indeks(glm::vec3 indeks) {
		indeksi.push_back((unsigned int)indeks.x);
		indeksi.push_back((unsigned int)indeks.y);
		indeksi.push_back((unsigned int)indeks.z);
	}

	std::pair< glm::vec3, glm::vec3 > getBoundingBox() {
		if (vrhovi.size() < 1) {
			glm::vec3 nil(0, 0, 0);
			return std::make_pair(nil, nil);
		}

		glm::vec3 mini(vrhovi[0], vrhovi[1], vrhovi[2]);
		glm::vec3 maxi(vrhovi[0], vrhovi[1], vrhovi[2]);

		int n = vrhovi.size();

		for (int i = 0; i < n / 4; i++) {

			if (vrhovi[i * 4] < mini.x) {
				mini.x = vrhovi[i * 4];
			}
			if (vrhovi[i * 4] >= maxi.x) {
				maxi.x = vrhovi[i * 4];
			}

			if (vrhovi[i * 4 + 1] < mini.y) {
				mini.y = vrhovi[i * 4 + 1];
			}
			if (vrhovi[i * 4 + 1] >= maxi.y) {
				maxi.y = vrhovi[i * 4 + 1];
			}

			if (vrhovi[i * 4 + 2] < mini.z) {
				mini.z = vrhovi[i * 4 + 2];
			}
			if (vrhovi[i * 4 + 2] >= maxi.z) {
				maxi.z = vrhovi[i * 4 + 2];
			}
		}

		return std::make_pair(mini, maxi);
	}

	int getVertSize() {
		return vrhovi.size();
	}

	int getIndeksSize() {
		return indeksi.size();
	}

	void applyTransform(glm::mat4 new_transform) {
		transform = new_transform * transform;

		int n = vrhovi.size();

		for (int i = 0; i < n; i += 4) {
			glm::vec4 vrh;
			vrh.x = vrhovi[i];
			vrh.y = vrhovi[i + 1];
			vrh.z = vrhovi[i + 2];
			vrh.a = vrhovi[i + 3];

			vrh = new_transform * vrh;

			if (i + 4 >= n) break;

			vrhovi[i] = vrh.x;
			vrhovi[i + 1] = vrh.y;
			vrhovi[i + 2] = vrh.z;
			vrhovi[i + 3] = vrh.a;
		}
	}

};

class Object {
public:
	Shader* shader;
	GLuint VAO;
	GLuint VBO[2];
	GLuint EBO;
	Mesh poligon;
	GLint model_loc;
	GLint view_loc;
	GLint projection_loc;
	GLint eye_loc;
	glm::mat4 frustum;
	glm::vec3 eye = glm::vec3(3.0f, 4.0f, 1.0f);

	Object(Shader* shader_tmp) {
		shader = shader_tmp;
		float aspect = (float)width / (float)height;

		// KLJUČ: Zadržite isti vertikalni FOV, pustite da se horizontalni mijenja
		float vertical_fov = 45.0f; // Ovo ostaje konstantno

		// Za frustum:
		float near_plane = 0.1f;
		float far_plane = 100.0f;

		// Izračunajte frustum parametre
		float top = near_plane * tan(glm::radians(vertical_fov) / 2.0f);
		float bottom = -top;
		float right = top * aspect;  // Ovo se mijenja s aspect ratio-om
		float left = -right;

		frustum = glm::frustum(left, right, bottom, top, near_plane, far_plane);
		glGenVertexArrays(1, &VAO);
		glGenBuffers(2, VBO);
		glGenBuffers(1, &EBO);
	}

	void reload() {
		model_loc = glGetUniformLocation(shader->ID, "model");
		view_loc = glGetUniformLocation(shader->ID, "view");
		projection_loc = glGetUniformLocation(shader->ID, "project");

		glBindVertexArray(VAO);
		//buffer za koordinate i povezi s nultim mjestom u sjencaru -- layout (location = 0)
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * poligon.getVertSize(), &poligon.vrhovi[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

		//buffer za boje i povezi s prvim mjestom u sjencaru -- layout (location = 1)
		glBindBuffer(GL_ARRAY_BUFFER, VBO[1]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * poligon.getVertSize(), &poligon.boje[0], GL_STATIC_DRAW);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		//buffer za indekse, moze biti samo jedan GL_ELEMENT_ARRAY_BUFFER po VAO

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * poligon.getIndeksSize(), &poligon.indeksi[0], GL_STATIC_DRAW);

		glBindVertexArray(0);

	}

	void draw() {
		glUseProgram(shader->ID);

		glBindVertexArray(VAO);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(poligon.getModelMatrix()));
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(poligon.getViewMatrix()));
		glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(frustum));

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDrawElements(GL_TRIANGLES, poligon.getIndeksSize(), GL_UNSIGNED_INT, 0);

		glBindVertexArray(0);
	}


	~Object() {
		delete shader;

		glDeleteBuffers(2, VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteVertexArrays(1, &VAO);
	}
};

class Spline {
public:
	std::vector<float> spline_tocke;
	std::vector < glm::vec3 > spline;
	std::vector <glm::vec3> spline_tangent_lines;
	std::vector < glm::vec3 > segment_tangent_vectors;
	Shader* shader;
	GLuint VAO;
	GLuint VBO[2];
	GLuint EBO;
	GLuint VAO_tangents;
	GLuint VBO_tangents;
	Mesh poligon;
	GLint model_loc;
	GLint view_loc;
	GLint projection_loc;
	GLint eye_loc;
	glm::mat4 frustum;
	glm::vec3 eye = glm::vec3(3.0f, 4.0f, 1.0f);

	Spline(Shader* shader_tmp) {
		create_default();
		normalize_default();
		//calculate_orientations();
		create_spline();
		//create_tangents();

		shader = shader_tmp;

		float aspect = (float)width / (float)height;

		// KLJUČ: Zadržite isti vertikalni FOV, pustite da se horizontalni mijenja
		float vertical_fov = 45.0f; // Ovo ostaje konstantno

		// Za frustum:
		float near_plane = 0.1f;
		float far_plane = 100.0f;

		// Izračunajte frustum parametre
		float top = near_plane * tan(glm::radians(vertical_fov) / 2.0f);
		float bottom = -top;
		float right = top * aspect;  // Ovo se mijenja s aspect ratio-om
		float left = -right;

		frustum = glm::frustum(left, right, bottom, top, near_plane, far_plane);
		glGenVertexArrays(1, &VAO);
		glGenBuffers(2, VBO);

		glGenVertexArrays(1, &VAO_tangents);
		glGenBuffers(1, &VBO_tangents);
	}

	void reload() {
		model_loc = glGetUniformLocation(shader->ID, "model");
		view_loc = glGetUniformLocation(shader->ID, "view");
		projection_loc = glGetUniformLocation(shader->ID, "project");

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
		glBufferData(GL_ARRAY_BUFFER, spline.size() * sizeof(glm::vec3), &spline[0], GL_STATIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

		glEnableVertexAttribArray(0);

		glBindVertexArray(0);

		glBindVertexArray(VAO_tangents);
		glBindBuffer(GL_ARRAY_BUFFER, VBO_tangents);

		glBufferData(GL_ARRAY_BUFFER,
			spline_tangent_lines.size() * sizeof(glm::vec3),
			spline_tangent_lines.data(),
			GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(0);

		glDisableVertexAttribArray(1);
		glVertexAttrib3f(1, 1.0f, 0.0f, 0.0f);

		glBindVertexArray(0);

	}

	void draw() {
		glUseProgram(shader->ID);

		glm::mat4 tmp_model = glm::mat4(1.0f);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(poligon.getModelMatrix()));
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(poligon.getViewMatrix()));
		glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(frustum));


		glBindVertexArray(VAO);

		glDrawArrays(GL_LINE_STRIP, 0, spline.size());

		glBindVertexArray(0);

		glBindVertexArray(VAO_tangents);

		glDrawArrays(GL_LINES, 0, spline_tangent_lines.size());

		glBindVertexArray(0);
	}

	void create_default() {
		spline_tocke = {
			0.0f, 0.0f, 0.0f,
			0.0f, 10.0f, 5.0f,
			10.0f, 10.0f, 10.0f,
			10.0f, 0.0f, 15.0f,
			0.0f, 0.0f, 20.0f,
			0.0f, 10.0f, 25.0f,
			10.0f, 10.0f, 30.0f,
			10.0f, 0.0f, 35.0f,
			0.0f, 0.0f, 40.0f,
			0.0f, 10.0f, 45.0f,
			10.0f, 10.0f, 50.0f,
			10.0f, 0.0f, 55.0f
		};
	}

	void normalize_default() {
		for (int i = 0; i < spline_tocke.size(); i++) {
			spline_tocke[i] /= 4.0f;
		}
	}

	void create_spline() {
		int n = spline_tocke.size() / 3;

		// Trebamo barem 4 kontrolne točke za kubni B-spline
		if (n < 4) return;

		for (int index = 1; index <= n - 3; index++) {
			// Ispravljena B-spline matrica za kubni B-spline
			glm::mat4 B = glm::mat4(
				-1.0f / 6.0f, 3.0f / 6.0f, -3.0f / 6.0f, 1.0f / 6.0f,
				3.0f / 6.0f, -6.0f / 6.0f, 0.0f / 6.0f, 4.0f / 6.0f,
				-3.0f / 6.0f, 3.0f / 6.0f, 3.0f / 6.0f, 1.0f / 6.0f,
				1.0f / 6.0f, 0.0f / 6.0f, 0.0f / 6.0f, 0.0f / 6.0f
			);

			glm::vec3 P0 = getPoint(spline_tocke, index - 1);
			glm::vec3 P1 = getPoint(spline_tocke, index + 0);
			glm::vec3 P2 = getPoint(spline_tocke, index + 1);
			glm::vec3 P3 = getPoint(spline_tocke, index + 2);

			glm::vec4 Px(P0.x, P1.x, P2.x, P3.x);
			glm::vec4 Py(P0.y, P1.y, P2.y, P3.y);
			glm::vec4 Pz(P0.z, P1.z, P2.z, P3.z);

			float t = 0.0f;
			float pomak = 0.01f;

			while (t <= 1.0f) {
				glm::vec4 T = glm::vec4(t * t * t, t * t, t, 1);

				// Izračun pozicije na krivulji
				float x = glm::dot(T, B * Px);
				float y = glm::dot(T, B * Py);
				float z = glm::dot(T, B * Pz);

				glm::vec3 coordinates = glm::vec3(x, y, z);
				spline.emplace_back(coordinates);

				float t2 = t * t;

				glm::vec3 tangent =
					0.5f * (
						(-P0 + P2) +
						2.0f * (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * t +
						3.0f * (-P0 + 3.0f * P1 - 3.0f * P2 + P3) * t2
						);

				segment_tangent_vectors.emplace_back(tangent);

				if (t == 0) {
					tangent = glm::normalize(tangent);
					glm::vec3 new_coord = coordinates + tangent;

					spline_tangent_lines.push_back(coordinates);
					spline_tangent_lines.push_back(new_coord);
				}
				t += pomak;
			}
		}
	}

	std::vector<glm::vec3>get_spline() {
		return spline;
	}

	glm::vec3 get_spline_point(int index) {
		if (index >= spline.size()) {
			return glm::vec3(NULL);
		}
		return spline[index];
	}

	int get_spline_size() {
		return spline.size();
	}

	glm::vec3 getPoint(const std::vector<float>& spline_tocke, int i) {
		return glm::vec3(spline_tocke[i * 3 + 0],
			spline_tocke[i * 3 + 1],
			spline_tocke[i * 3 + 2]);
	}


	glm::vec3 get_forward(int index) {
		glm::vec3 forward = segment_tangent_vectors[index];
		return forward;
	}

	glm::vec3 get_rotation_axis(int index) {
		if (index >= segment_tangent_vectors.size()) {
			std::cout << "Exited out of range on rotation axis on index: " << index << "\n";
			return glm::vec3(NULL);
		}
		glm::vec3 forward = glm::normalize(segment_tangent_vectors[index]);
		glm::vec3 s = glm::vec3(0, 0, 1);

		glm::vec3 axis = glm::normalize(glm::cross(s, forward));

		return axis;
	}

	float get_rotation_angle(int index) {
		if (index >= segment_tangent_vectors.size()) {
			std::cout << "Exited out of range on rotation angle on index: " << index << "\n";
			return 0.0f;
		}
		glm::vec3 forward = glm::normalize(segment_tangent_vectors[index]);
		glm::vec3 s = glm::vec3(0, 0, 1);

		float angle = acos(glm::clamp(glm::dot(s, forward), -1.0f, 1.0f));

		return angle;
	}

	~Spline() {
		delete shader;

		glDeleteBuffers(2, VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteVertexArrays(1, &VAO);
	}
};

struct Particle {
	glm::vec3 pos;
	glm::vec3 vel;
	float age = 0.0f;
	float life = 1.0f;
	glm::vec4 color = glm::vec4(1, 1, 1, 1);
	bool alive = false;
};

class ParticleEngine {
public:
	Shader* shader = nullptr;

	GLuint VAO = 0;
	GLuint VBO_pos = 0;
	GLuint VBO_col = 0;

	std::vector<Particle> p;
	std::vector<glm::vec3> gpuPos;
	std::vector<glm::vec4> gpuCol;

	glm::vec3 emitterPos = glm::vec3(0, 0, 0);
	float emitRate = 200.0f;       // particles / s
	float emitAccumulator = 0.0f;

	GLuint tex = 0;

	ParticleEngine(Shader* sh, int maxCount)
	{
		shader = sh;
		p.resize(maxCount);
		gpuPos.reserve(maxCount);
		gpuCol.reserve(maxCount);

		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);

		// positions -> location 0 (vec3)
		glGenBuffers(1, &VBO_pos);
		glBindBuffer(GL_ARRAY_BUFFER, VBO_pos);
		glBufferData(GL_ARRAY_BUFFER, maxCount * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(0);

		// colors -> location 1 (vec4)
		glGenBuffers(1, &VBO_col);
		glBindBuffer(GL_ARRAY_BUFFER, VBO_col);
		glBufferData(GL_ARRAY_BUFFER, maxCount * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}

	void respawn(Particle& x) {
		x.alive = true;
		x.age = 0.0f;
		x.life = 1.0f + (rand() / (float)RAND_MAX) * 1.5f;
		x.pos = emitterPos;

		glm::vec3 dir(
			(rand() / (float)RAND_MAX) * 2.0f - 1.0f,
			(rand() / (float)RAND_MAX) * 2.0f - 1.0f,
			(rand() / (float)RAND_MAX) * 2.0f - 1.0f
		);
		if (glm::length(dir) < 1e-5f) dir = glm::vec3(0, 1, 0);
		dir = glm::normalize(dir);

		float speed = 1.0f + (rand() / (float)RAND_MAX) * 2.0f;
		x.vel = dir * speed;

		x.color = glm::vec4(1, 1, 1, 1);
	}

	void update(float dt) {
		// 1) age + kill + integrate
		for (auto& x : p) {
			if (!x.alive) continue;

			x.age += dt;
			if (x.age >= x.life) { x.alive = false; continue; }

			// primjer sile: gravitacija
			glm::vec3 a(0.0f, -3.0f, 0.0f);
			x.vel += a * dt;
			x.pos += x.vel * dt;

			// fade out
			float t = x.age / x.life;
			x.color.a = 1.0f - t;
		}

		// 2) spawn
		emitAccumulator += dt * emitRate;
		int spawnCount = (int)emitAccumulator;
		emitAccumulator -= spawnCount;

		for (int i = 0; i < spawnCount; ++i) {
			for (auto& x : p) {
				if (!x.alive) { respawn(x); break; }
			}
		}
	}

	void uploadToGPU() {
		gpuPos.clear();
		gpuCol.clear();

		for (auto& x : p) {
			if (!x.alive) continue;
			gpuPos.push_back(x.pos);
			gpuCol.push_back(x.color);
		}

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO_pos);
		glBufferSubData(GL_ARRAY_BUFFER, 0, gpuPos.size() * sizeof(glm::vec3), gpuPos.data());

		glBindBuffer(GL_ARRAY_BUFFER, VBO_col);
		glBufferSubData(GL_ARRAY_BUFFER, 0, gpuCol.size() * sizeof(glm::vec4), gpuCol.data());

		glBindVertexArray(0);
	}

	void draw(const glm::mat4& view, const glm::mat4& proj)
	{
		glUseProgram(shader->ID);

		glm::mat4 model(1.0f);
		glUniformMatrix4fv(glGetUniformLocation(shader->ID, "model"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(shader->ID, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(shader->ID, "project"), 1, GL_FALSE, glm::value_ptr(proj));

		glUniform1f(glGetUniformLocation(shader->ID, "uPointSize"), 32.0f);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform1i(glGetUniformLocation(shader->ID, "uTex"), 0);

		glEnable(GL_PROGRAM_POINT_SIZE);

		glBindVertexArray(VAO);
		glDrawArrays(GL_POINTS, 0, (GLsizei)gpuPos.size());
		glBindVertexArray(0);

		glBindTexture(GL_TEXTURE_2D, 0);
	}



	~ParticleEngine() {
		glDeleteBuffers(1, &VBO_pos);
		glDeleteBuffers(1, &VBO_col);
		glDeleteVertexArrays(1, &VAO);
	}
};


int main(int argc, char* argv[]) {

	load_object(argv[0], "bird.obj");

	GLFWwindow* window;

	glfwInit();

	window = glfwCreateWindow(width, height, "neindeksirano/indeksirano/instancirano", nullptr, nullptr);

	// Check for Valid Context
	if (window == nullptr) {
		fprintf(stderr, "Failed to Create OpenGL Context");
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);

	gladLoadGL();

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback); //funkcija koja se poziva prilikom mijenjanja velicine prozora

	glClearColor(0, 0, 0, 1);

	Shader* shader = loadShader(argv[0], "shader");
	Shader* shader0 = loadShader(argv[0], "shader0");

	Spline spline(shader0);
	ParticleEngine pe(shader, 5000);

	GLuint texture = loadTextureRGBA(argv[0], "spark.jpg");

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


	spline.reload();
	pe.tex = texture;
	pe.emitRate = 30.0f;


	double lastTime = glfwGetTime();

	//glavna petlja programa
	while (glfwWindowShouldClose(window) == false) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		double now = glfwGetTime();
		float dt = (float)(now - lastTime);
		lastTime = now;

		spline.draw();

		static int si = 0;
		pe.emitterPos = spline.get_spline_point(si);
		si = (si + 1) % spline.get_spline_size();

		// update + upload + draw čestica
		pe.update(dt);
		pe.uploadToGPU();

		pe.draw(spline.poligon.getViewMatrix(), spline.frustum);


		glfwSwapBuffers(window);
		glfwPollEvents();
	}


	glfwTerminate();


	return EXIT_SUCCESS;
}
