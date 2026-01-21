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
#include <filesystem>


// Standard Headers
#include <string>
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <array>
#include <iostream>
#include "main.h"
#include <glm/gtc/type_ptr.hpp>

int width = 900, height = 900;
std::vector< float > load_vrhovi, load_boje;
std::vector< unsigned int > load_indeksi;

std::vector<float> g_vertices;
std::vector<unsigned int> g_indices;
std::string g_diffuseTexturePath;

std::vector<float> g_dart_vertices;
std::vector<unsigned int> g_dart_indices;
std::string g_dart_diffuseTexturePath;

double g_mouseX = 0.0, g_mouseY = 0.0;
bool   g_fireRequested = false;

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
	g_mouseX = xpos;
	g_mouseY = ypos;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
		g_fireRequested = true;
}

static bool isAbsolutePath(const std::string& p)
{
	if (p.size() >= 2 && std::isalpha((unsigned char)p[0]) && p[1] == ':') return true; // "C:"
	if (!p.empty() && (p[0] == '/' || p[0] == '\\')) return true; // unix ili UNC-like
	return false;
}

static std::string normalizeSlashes(std::string s)
{
	for (auto& c : s) if (c == '\\') c = '/';
	return s;
}

static std::string joinPath(const std::string& a, const std::string& b)
{
	if (a.empty()) return b;
	if (b.empty()) return a;
	if (a.back() == '/' || a.back() == '\\') return a + b;
	return a + "/" + b;
}

std::string getResourcesDir(char* argv0)
{
	std::string path(argv0);
	size_t slash = path.find_last_of("\\/");
	std::string dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
	return dir + "/resources/";
}

std::string getFilenameOnly(const std::string& p)
{
	size_t slash = p.find_last_of("/\\");
	if (slash == std::string::npos) return p;
	return p.substr(slash + 1);
}

static glm::vec3 bezier3(const glm::vec3& P0, const glm::vec3& P1, const glm::vec3& P2, const glm::vec3& P3, float t)
{
	float u = 1.0f - t;
	return (u * u * u) * P0
		+ (3.0f * u * u * t) * P1
		+ (3.0f * u * t * t) * P2
		+ (t * t * t) * P3;
}

static glm::vec3 bezier3_tangent(const glm::vec3& P0, const glm::vec3& P1, const glm::vec3& P2, const glm::vec3& P3, float t)
{
	// derivacija kubnog Bezier-a
	float u = 1.0f - t;
	return (3.0f * u * u) * (P1 - P0)
		+ (6.0f * u * t) * (P2 - P1)
		+ (3.0f * t * t) * (P3 - P2);
}

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

bool load_fbx(char* argv0, const std::string& objekt)
{
	g_vertices.clear();
	g_indices.clear();
	g_diffuseTexturePath.clear();

	Assimp::Importer importer;

	std::string resDir = getResourcesDir(argv0);
	std::string fbxPath = resDir + objekt;

	//std::cout << "Trying FBX: " << fbxPath << "\n";

	const aiScene* scene = importer.ReadFile(
		fbxPath.c_str(),
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices |
		aiProcess_FlipUVs
	);

	if (!scene || !scene->mRootNode) {
		std::cerr << "Assimp error: " << importer.GetErrorString() << "\n";
		return false;
	}

	unsigned int vertexOffset = 0;

	for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
	{
		aiMesh* mesh = scene->mMeshes[m];

		// ===== vertices =====
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
			g_vertices.push_back(mesh->mVertices[i].x);
			g_vertices.push_back(mesh->mVertices[i].y);
			g_vertices.push_back(mesh->mVertices[i].z);

			aiVector3D n = mesh->HasNormals()
				? mesh->mNormals[i]
				: aiVector3D(0, 0, 1);

				g_vertices.push_back(n.x);
				g_vertices.push_back(n.y);
				g_vertices.push_back(n.z);

				if (mesh->HasTextureCoords(0)) {
					g_vertices.push_back(mesh->mTextureCoords[0][i].x);
					g_vertices.push_back(mesh->mTextureCoords[0][i].y);
				}
				else {
					g_vertices.push_back(0.0f);
					g_vertices.push_back(0.0f);
				}
		}

		// ===== indices =====
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices != 3) continue;

			g_indices.push_back(vertexOffset + face.mIndices[0]);
			g_indices.push_back(vertexOffset + face.mIndices[1]);
			g_indices.push_back(vertexOffset + face.mIndices[2]);
		}

		// ===== TEXTURA: SAMO FILENAME =====
		if (g_diffuseTexturePath.empty() && mesh->mMaterialIndex >= 0)
		{
			aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
			aiString texPath;

			if (mat && mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
			{
				std::string filename = getFilenameOnly(texPath.C_Str());
				g_diffuseTexturePath = resDir + filename;

				//std::cout << "Resolved texture: " << g_diffuseTexturePath << "\n";
			}
		}

		vertexOffset += mesh->mNumVertices;
	}

	/*std::cout << "[FBX loaded]\n";
	std::cout << "Vertices: " << g_vertices.size() / 8 << "\n";
	std::cout << "Indices : " << g_indices.size() << "\n";
	std::cout << "Texture : " << g_diffuseTexturePath << "\n";*/

	return !g_vertices.empty();
}

static std::string trimCopy(std::string s)
{
	auto isws = [](unsigned char c) { return std::isspace(c); };
	while (!s.empty() && isws((unsigned char)s.front())) s.erase(s.begin());
	while (!s.empty() && isws((unsigned char)s.back()))  s.pop_back();
	return s;
}

static std::string dirnameOf(const std::string& p)
{
	size_t slash = p.find_last_of("/\\");
	return (slash == std::string::npos) ? "." : p.substr(0, slash);
}

static std::string filenameOnly(const std::string& p)
{
	size_t slash = p.find_last_of("/\\");
	return (slash == std::string::npos) ? p : p.substr(slash + 1);
}

// Pokuša iz .mtl izvući map_Kd (diffuse teksturu)
static std::string parseMtlDiffuseTexture(const std::string& mtlFullPath)
{
	std::ifstream f(mtlFullPath);
	if (!f.is_open()) return "";

	std::string line;
	while (std::getline(f, line))
	{
		line = trimCopy(line);
		if (line.rfind("map_Kd", 0) == 0) // starts with map_Kd
		{
			// map_Kd <path>
			std::string rest = trimCopy(line.substr(6));
			// ako ima navodnike, skini ih
			if (!rest.empty() && rest.front() == '"') {
				rest.erase(rest.begin());
				size_t q = rest.find('"');
				if (q != std::string::npos) rest = rest.substr(0, q);
			}
			return rest;
		}
	}
	return "";
}

bool load_dart_object(char* argv0, const std::string& objektObj)
{
	g_dart_vertices.clear();
	g_dart_indices.clear();
	g_dart_diffuseTexturePath.clear();

	Assimp::Importer importer;

	std::string resDir = getResourcesDir(argv0);          // npr ".../Debug/resources/"
	std::string objPath = resDir + objektObj;             // ".../resources/dart.obj"

	//std::cout << "Trying DART OBJ: " << objPath << "\n";

	const aiScene* scene = importer.ReadFile(
		objPath.c_str(),
		aiProcess_Triangulate |
		aiProcess_GenSmoothNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_JoinIdenticalVertices |
		aiProcess_FlipUVs
	);

	if (!scene || !scene->mRootNode)
	{
		std::cerr << "Assimp error (dart): " << importer.GetErrorString() << "\n";
		return false;
	}

	// ===== Ucitaj meshove =====
	unsigned int vertexOffset = 0;

	for (unsigned int m = 0; m < scene->mNumMeshes; ++m)
	{
		aiMesh* mesh = scene->mMeshes[m];

		// vertices: pos3 normal3 uv2
		for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
		{
			// pos
			g_dart_vertices.push_back(mesh->mVertices[i].x);
			g_dart_vertices.push_back(mesh->mVertices[i].y);
			g_dart_vertices.push_back(mesh->mVertices[i].z);

			// normal
			aiVector3D n = mesh->HasNormals() ? mesh->mNormals[i] : aiVector3D(0, 0, 1);
			g_dart_vertices.push_back(n.x);
			g_dart_vertices.push_back(n.y);
			g_dart_vertices.push_back(n.z);

			// uv
			if (mesh->HasTextureCoords(0))
			{
				g_dart_vertices.push_back(mesh->mTextureCoords[0][i].x);
				g_dart_vertices.push_back(mesh->mTextureCoords[0][i].y);
			}
			else
			{
				g_dart_vertices.push_back(0.0f);
				g_dart_vertices.push_back(0.0f);
			}
		}

		// indices
		for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices != 3) continue;

			g_dart_indices.push_back(vertexOffset + face.mIndices[0]);
			g_dart_indices.push_back(vertexOffset + face.mIndices[1]);
			g_dart_indices.push_back(vertexOffset + face.mIndices[2]);
		}

		// texture: probaj iz assimp materijala
		if (g_dart_diffuseTexturePath.empty() && mesh->mMaterialIndex >= 0)
		{
			aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
			aiString texPath;

			if (mat && mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
			{
				std::string filename = filenameOnly(texPath.C_Str());
				g_dart_diffuseTexturePath = resDir + filename;
			}
		}

		vertexOffset += mesh->mNumVertices;
	}

	// Ako assimp nije dao teksturu, probaj pročitati .mtl
	if (g_dart_diffuseTexturePath.empty())
	{
		// Pretpostavka: dart.mtl je u resources i zove se "dart.mtl"
		// Ako se zove drugačije, samo promijeni ovdje.
		std::string mtlFullPath = resDir + "dart.mtl";
		std::string mapKd = parseMtlDiffuseTexture(mtlFullPath);
		if (!mapKd.empty())
		{
			// u .mtl često bude samo ime fajla
			g_dart_diffuseTexturePath = resDir + filenameOnly(mapKd);
		}
	}

	/*std::cout << "[DART loaded]\n";
	std::cout << "Vertices: " << (g_dart_vertices.size() / 8) << "\n";
	std::cout << "Indices : " << g_dart_indices.size() << "\n";
	std::cout << "Texture : " << g_dart_diffuseTexturePath << "\n";*/

	return !g_dart_vertices.empty() && !g_dart_indices.empty();
}

GLuint loadTextureFromFile(const std::string& fullPath)
{
	stbi_set_flip_vertically_on_load(true);

	int w = 0, h = 0, n = 0;
	unsigned char* data = stbi_load(fullPath.c_str(), &w, &h, &n, 0); // 0 = original channels
	if (!data) {
		std::cerr << "Texture load failed: " << fullPath << "\n";
		std::cerr << "stbi: " << stbi_failure_reason() << "\n";
		return 0;
	}

	//std::cout << "Loaded texture channels (n): " << n << "\n"; // <-- KLJUČ

	GLenum srcFmt = (n == 4) ? GL_RGBA : (n == 3 ? GL_RGB : GL_RGBA);
	GLenum dstFmt = (n == 4) ? GL_RGBA8 : GL_RGB8;

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, dstFmt, w, h, 0, srcFmt, GL_UNSIGNED_BYTE, data);

	// za test: bez mipmapa
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	stbi_image_free(data);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

GLuint loadTextureRGBA(const std::string& putanja, const std::string& objekt)
{
	std::string path(putanja);
	size_t slash = path.find_last_of("\\/");
	std::string dir = (slash == std::string::npos) ? "." : path.substr(0, slash);
	std::string fullPath = dir + "/resources/" + objekt;

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
	glm::vec3 camPosition = glm::vec3(0.0f, 0.0f, 8.0f); // ispred ploče
	glm::vec3 camTarget = glm::vec3(0.0f, 0.0f, 0.0f);  // gleda u centar
	glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

	glm::mat4 view = glm::lookAt(
		camPosition,
		camTarget,
		worldUp
	);
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

// Spline interpolacijska B krivulja
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

	void draw(const glm::mat4& view, const glm::mat4& proj) {
		glUseProgram(shader->ID);

		glm::mat4 tmp_model = glm::mat4(1.0f);
		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(poligon.getModelMatrix()));
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(proj));


		glBindVertexArray(VAO);

		glDrawArrays(GL_LINE_STRIP, 0, spline.size());

		glBindVertexArray(0);

		glBindVertexArray(VAO_tangents);

		glDrawArrays(GL_LINES, 0, spline_tangent_lines.size());

		glBindVertexArray(0);
	}

	void create_default() {
		spline_tocke = {
			0.0f, -0.5f, 4.0f,
			0.0f, 3.5f, 2.0f,
			-0.043f, 3.5f, 0.31f,
			-0.043f, 1.91f, -1.31f
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
				-0.5f, 1.5f, -1.5f, 0.5f,
				1.0f, -2.5f, 2.0f, -0.5f,
				-0.5f, 0.0f, 0.5f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f
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

// Struktura mreza za validaciju greske
class DartBoard {
public:
	Shader* shader = nullptr;

	GLuint VAO_circles = 0;
	GLuint VBO_circles = 0;

	GLuint VAO_lines = 0;
	GLuint VBO_lines = 0;

	GLint model_loc = -1;
	GLint view_loc = -1;
	GLint projection_loc = -1;

	Mesh poligon;
	glm::mat4 frustum;

	float R = 1.0f;
	int segments = 192;

	float r_double_out = 1.00f;
	float r_double_in = 0.92f;

	float r_triple_out = 0.62f;
	float r_triple_in = 0.56f;

	float r_outer_bull = 0.10f;
	float r_bull = 0.05f;

	std::vector<glm::vec3> circlesPacked;
	std::vector<glm::vec3> sectorLines;

	int off_doubleOut = 0;
	int off_doubleIn = 0;
	int off_tripleOut = 0;
	int off_tripleIn = 0;
	int off_outerBull = 0;
	int off_bull = 0;

	glm::mat4 boardModel = glm::mat4(1.0f);

	DartBoard(Shader* shader_tmp,
		int segments_tmp = 192)
	{
		shader = shader_tmp;
		segments = segments_tmp;

		boardModel = glm::rotate(boardModel, glm::radians(9.0f), glm::vec3(0, 0, 1));

		// Frustum kao u Spline (isti stil)
		float aspect = (float)width / (float)height;
		float vertical_fov = 45.0f;
		float near_plane = 0.1f;
		float far_plane = 100.0f;

		float top = near_plane * tan(glm::radians(vertical_fov) / 2.0f);
		float bottom = -top;
		float right = top * aspect;
		float left = -right;

		frustum = glm::frustum(left, right, bottom, top, near_plane, far_plane);

		glGenVertexArrays(1, &VAO_circles);
		glGenBuffers(1, &VBO_circles);

		glGenVertexArrays(1, &VAO_lines);
		glGenBuffers(1, &VBO_lines);

		create_geometry();
	}

	void setBoardModel(const glm::mat4& m) { boardModel = m; }

	void append_circle(float radius, int& outOffset)
	{
		outOffset = (int)circlesPacked.size();
		circlesPacked.reserve(circlesPacked.size() + segments);

		for (int i = 0; i < segments; i++) {
			float a = (float)i / (float)segments * 2.0f * 3.1415926535f;
			circlesPacked.push_back(glm::vec3(cos(a) * radius, sin(a) * radius, 0.0f));
		}
	}

	void create_geometry()
	{
		circlesPacked.clear();
		sectorLines.clear();

		// 6 krugova
		append_circle(R * r_double_out, off_doubleOut);
		append_circle(R * r_double_in, off_doubleIn);

		append_circle(R * r_triple_out, off_tripleOut);
		append_circle(R * r_triple_in, off_tripleIn);

		append_circle(R * r_outer_bull, off_outerBull);
		append_circle(R * r_bull, off_bull);

		// 20 sektora (radijalne linije): centar -> vanjski rub double-a
		sectorLines.reserve(20 * 2);
		for (int s = 0; s < 20; s++) {
			float a = (float)s / 20.0f * 2.0f * 3.1415926535f;
			sectorLines.push_back(glm::vec3(0, 0, 0));
			sectorLines.push_back(glm::vec3(cos(a) * R * r_double_out, sin(a) * R * r_double_out, 0.0f));
		}
	}

	void reload()
	{
		model_loc = glGetUniformLocation(shader->ID, "model");
		view_loc = glGetUniformLocation(shader->ID, "view");
		projection_loc = glGetUniformLocation(shader->ID, "project");

		// circles VAO/VBO
		glBindVertexArray(VAO_circles);
		glBindBuffer(GL_ARRAY_BUFFER, VBO_circles);
		glBufferData(GL_ARRAY_BUFFER,
			circlesPacked.size() * sizeof(glm::vec3),
			circlesPacked.data(),
			GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);

		// lines VAO/VBO
		glBindVertexArray(VAO_lines);
		glBindBuffer(GL_ARRAY_BUFFER, VBO_lines);
		glBufferData(GL_ARRAY_BUFFER,
			sectorLines.size() * sizeof(glm::vec3),
			sectorLines.data(),
			GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glEnableVertexAttribArray(0);
		glBindVertexArray(0);
	}

	void draw()
	{
		glUseProgram(shader->ID);

		// Model: kombiniraj "mesh model" i boardModel ako želiš
		glm::mat4 model = poligon.getModelMatrix() * boardModel;

		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(poligon.getViewMatrix()));
		glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(frustum));

		// krugovi (6 loopova)
		glBindVertexArray(VAO_circles);
		glDrawArrays(GL_LINE_LOOP, off_doubleOut, segments);
		glDrawArrays(GL_LINE_LOOP, off_doubleIn, segments);
		glDrawArrays(GL_LINE_LOOP, off_tripleOut, segments);
		glDrawArrays(GL_LINE_LOOP, off_tripleIn, segments);
		glDrawArrays(GL_LINE_LOOP, off_outerBull, segments);
		glDrawArrays(GL_LINE_LOOP, off_bull, segments);
		glBindVertexArray(0);

		// sektorske linije
		glBindVertexArray(VAO_lines);
		glDrawArrays(GL_LINES, 0, (GLsizei)sectorLines.size());
		glBindVertexArray(0);
	}

	glm::vec3 worldToBoard(const glm::vec3& worldPoint) const
	{
		// isti model kao u draw()
		glm::mat4 model = poligon.model * boardModel;
		glm::mat4 inv = glm::inverse(model);
		glm::vec4 p = inv * glm::vec4(worldPoint, 1.0f);
		return glm::vec3(p);
	}

	bool hitTestLocal(const glm::vec3& localPoint) const
	{
		float r = glm::length(glm::vec2(localPoint.x, localPoint.y));
		return r <= (R * r_double_out);
	}

	int sectorValueFromAngle(float angleRad) const
	{
		float a = glm::half_pi<float>() - angleRad;

		const float twoPi = 2.0f * glm::pi<float>();
		const float step = twoPi / 20.0f;

		a += step * 0.5f;

		while (a < 0.0f) a += twoPi;
		while (a >= twoPi) a -= twoPi;

		int sector = (int)floor(a / step) % 20;

		static int values[20] = {
			20, 1, 18, 4, 13,
			6, 10, 15, 2, 17,
			3, 19, 7, 16, 8,
			11, 14, 9, 12, 5
		};

		return values[sector];
	}

	int scoreAtLocal(const glm::vec3& localPoint) const
	{
		float x = localPoint.x;
		float y = localPoint.y;

		float r = sqrt(x * x + y * y);

		if (r > R * r_double_out) return 0; // miss

		// bull
		if (r <= R * r_bull) return 50;
		if (r <= R * r_outer_bull) return 25;

		float angle = atan2(y, x);
		if (angle < 0) angle += 2.0f * 3.1415926535f;

		int base = sectorValueFromAngle(angle);

		// double/triple
		if (r >= R * r_double_in && r <= R * r_double_out) return base * 2;
		if (r >= R * r_triple_in && r <= R * r_triple_out) return base * 3;

		return base; // single
	}

	bool hitTestWorld(const glm::vec3& worldPoint) const
	{
		return hitTestLocal(worldToBoard(worldPoint));
	}

	int scoreAtWorld(const glm::vec3& worldPoint) const
	{
		return scoreAtLocal(worldToBoard(worldPoint));
	}

	~DartBoard()
	{
		glDeleteBuffers(1, &VBO_circles);
		glDeleteVertexArrays(1, &VAO_circles);

		glDeleteBuffers(1, &VBO_lines);
		glDeleteVertexArrays(1, &VAO_lines);

		// Nemoj delete shader ovdje ako je shader shared u projektu.
		// delete shader;
	}
};

class TexturedDartboard {
public:
	Shader* shader = nullptr;

	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint EBO = 0;

	GLuint tex = 0;

	GLint model_loc = -1;
	GLint view_loc = -1;
	GLint projection_loc = -1;
	GLint tex_loc = -1;

	glm::mat4 frustum;
	glm::vec3 eye = glm::vec3(3.0f, 4.0f, 1.0f);

	// model matrica “ploče” u svijetu
	glm::mat4 model = glm::mat4(1.0f);

	// view možeš uzeti iz tvog Transform/Mesh sustava,
	// ali da bude kao Spline, ovdje držimo vlastiti eye (po želji)
	glm::vec3 camPosition = glm::vec3(0.0f, 0.0f, 12.0f);
	glm::vec3 camTarget = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

	TexturedDartboard(Shader* shader_tmp)
	{
		shader = shader_tmp;

		// frustum isto kao Spline
		float aspect = (float)width / (float)height;
		float vertical_fov = 45.0f;
		float near_plane = 0.1f;
		float far_plane = 200.0f;

		float top = near_plane * tan(glm::radians(vertical_fov) / 2.0f);
		float bottom = -top;
		float right = top * aspect;
		float left = -right;

		frustum = glm::frustum(left, right, bottom, top, near_plane, far_plane);

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
	}

	void setModel(const glm::mat4& m) { model = m; }

	void setCamera(const glm::vec3& pos, const glm::vec3& target, const glm::vec3& up)
	{
		camPosition = pos;
		camTarget = target;
		worldUp = up;
	}

	glm::mat4 getView() const
	{
		return glm::lookAt(camPosition, camTarget, worldUp);
	}

	// pozovi nakon load_fbx()
	bool buildFromGlobals()
	{
		if (g_vertices.empty() || g_indices.empty()) {
			std::cerr << "FBX globals empty: g_vertices/g_indices\n";
			return false;
		}

		if (!g_diffuseTexturePath.empty()) {
			tex = loadTextureFromFile(g_diffuseTexturePath);
			if (!tex) {
				std::cerr << "Warning: failed to load texture: " << g_diffuseTexturePath << "\n";
			}
		}
		else {
			std::cerr << "Warning: no diffuse texture path in FBX\n";
		}

		reload();
		return true;
	}

	void reload()
	{
		model_loc = glGetUniformLocation(shader->ID, "model");
		view_loc = glGetUniformLocation(shader->ID, "view");
		projection_loc = glGetUniformLocation(shader->ID, "project");
		tex_loc = glGetUniformLocation(shader->ID, "uTex");

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, g_vertices.size() * sizeof(float), g_vertices.data(), GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_indices.size() * sizeof(unsigned int), g_indices.data(), GL_STATIC_DRAW);

		// layout: pos3 normal3 uv2  => 8 floatova
		GLsizei stride = 8 * sizeof(float);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

	void draw(const glm::mat4& view, const glm::mat4& proj)
	{
		glUseProgram(shader->ID);

		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(proj));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform1i(tex_loc, 0);

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES,
			(GLsizei)g_indices.size(),
			GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}


	~TexturedDartboard()
	{
		if (tex) glDeleteTextures(1, &tex);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteVertexArrays(1, &VAO);
	}
};

class Dart {
public:
	Shader* shader = nullptr;

	glm::mat4 model = glm::mat4(1.0f);

	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint EBO = 0;
	GLuint tex = 0;

	GLint model_loc = -1;
	GLint view_loc = -1;
	GLint projection_loc = -1;
	GLint tex_loc = -1;

	// Transform (animiraš samo MODEL)
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotationDeg = glm::vec3(0.0f);   // Euler u stupnjevima
	glm::vec3 scale = glm::vec3(1.0f);

	// Animacija
	glm::vec3 velocity = glm::vec3(0.0f);
	bool active = false;

	Dart(Shader* shader_tmp)
	{
		shader = shader_tmp;

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
	}

	// ===== Transform setters =====
	void setPosition(const glm::vec3& p)
	{
		position = p;
	}
	void setScale(const glm::vec3& s)
	{
		scale = s;
	}
	void setRotationEulerDeg(const glm::vec3& rDeg)
	{
		rotationDeg = rDeg;
	}

	glm::mat4 getModel() const
	{
		glm::mat4 m(1.0f);
		m = glm::translate(m, position);
		m = glm::rotate(m, glm::radians(rotationDeg.x), glm::vec3(1, 0, 0));
		m = glm::rotate(m, glm::radians(rotationDeg.y), glm::vec3(0, 1, 0));
		m = glm::rotate(m, glm::radians(rotationDeg.z), glm::vec3(0, 0, 1));
		m = glm::scale(m, scale);
		return m;
	}

	// ===== Build iz dart globalnih varijabli =====
	bool buildFromGlobals()
	{
		if (g_dart_vertices.empty() || g_dart_indices.empty()) {
			std::cerr << "DART globals empty!\n";
			return false;
		}

		if (!g_dart_diffuseTexturePath.empty()) {
			tex = loadTextureFromFile(g_dart_diffuseTexturePath);
			if (!tex) {
				std::cerr << "Warning: dart texture failed: " << g_dart_diffuseTexturePath << "\n";
			}
		}
		else {
			std::cerr << "Warning: dart has no diffuse texture path\n";
		}

		reload();
		return true;
	}

	void reload()
	{
		model_loc = glGetUniformLocation(shader->ID, "model");
		view_loc = glGetUniformLocation(shader->ID, "view");
		projection_loc = glGetUniformLocation(shader->ID, "project");
		tex_loc = glGetUniformLocation(shader->ID, "uTex");

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER,
			g_dart_vertices.size() * sizeof(float),
			g_dart_vertices.data(),
			GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
			g_dart_indices.size() * sizeof(unsigned int),
			g_dart_indices.data(),
			GL_STATIC_DRAW);

		GLsizei stride = 8 * sizeof(float);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

	// KLJUČ: view i project dolaze izvana (isti kao na boardu!)
	void draw(const glm::mat4& view, const glm::mat4& proj)
	{
		glUseProgram(shader->ID);

		glm::mat4 model = getModel();

		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(proj));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform1i(tex_loc, 0);

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, (GLsizei)g_dart_indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		glBindTexture(GL_TEXTURE_2D, 0);
	}

	~Dart()
	{
		if (tex) glDeleteTextures(1, &tex);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteVertexArrays(1, &VAO);
	}
};

class AimBar {
public:
	Shader* shader = nullptr;

	GLuint VAO = 0;
	GLuint VBO = 0;

	GLint model_loc = -1;
	GLint view_loc = -1;
	GLint projection_loc = -1;

	float x0 = -0.85f;   // lijevo
	float x1 = -0.25f;   // desno
	float y0 = 0.85f;   // dolje
	float y1 = 0.90f;   // gore

	float safeMin = -0.3f;   // zelena zona u skali [-1,1]
	float safeMax = 0.3f;

	float value = 0.0f;      // očekuje [-1,1]

	std::vector<float> verts;

	AimBar(Shader* sh) : shader(sh)
	{
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}

	void setValue(float v) { value = glm::clamp(v, -1.0f, 1.0f); }

	void setRect(float _x0, float _x1, float _y0, float _y1) {
		x0 = _x0; x1 = _x1; y0 = _y0; y1 = _y1;
	}

	void setSafeZone(float a, float b) {
		safeMin = a; safeMax = b;
	}

	void reload()
	{
		model_loc = glGetUniformLocation(shader->ID, "model");
		view_loc = glGetUniformLocation(shader->ID, "view");
		projection_loc = glGetUniformLocation(shader->ID, "project");

		buildGeometry(); // inicijalni build

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);

		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindVertexArray(0);
	}

	void draw()
	{
		// rebuild svaki frame (zbog kazaljke)
		buildGeometry();

		glUseProgram(shader->ID);

		// ako shader nema te uniforme, lokacije će biti -1 i ovo se ignorira
		if (model_loc != -1)      glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
		if (view_loc != -1)       glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
		if (projection_loc != -1) glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());

		// overlay: bez deptha
		glDisable(GL_DEPTH_TEST);

		// crtamo sve kao trokute
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 5));

		glEnable(GL_DEPTH_TEST);
		glBindVertexArray(0);
	}

	~AimBar()
	{
		glDeleteBuffers(1, &VBO);
		glDeleteVertexArrays(1, &VAO);
	}

private:
	void pushTri(float ax, float ay, float bx, float by, float cx, float cy,
		float r, float g, float b)
	{
		// A
		verts.push_back(ax); verts.push_back(ay);
		verts.push_back(r);  verts.push_back(g);  verts.push_back(b);
		// B
		verts.push_back(bx); verts.push_back(by);
		verts.push_back(r);  verts.push_back(g);  verts.push_back(b);
		// C
		verts.push_back(cx); verts.push_back(cy);
		verts.push_back(r);  verts.push_back(g);  verts.push_back(b);
	}

	void pushRect(float rx0, float rx1, float ry0, float ry1,
		float r, float g, float b)
	{
		// 2 trokuta
		pushTri(rx0, ry0, rx1, ry0, rx1, ry1, r, g, b);
		pushTri(rx0, ry0, rx1, ry1, rx0, ry1, r, g, b);
	}

	void buildGeometry()
	{
		verts.clear();
		verts.reserve(6 * 5 * 10); // malo rezerve

		// mapiranje safe zone [-1,1] -> [x0,x1]
		auto mapX = [&](float v) {
			float t = (v + 1.0f) * 0.5f;          // 0..1
			return x0 + t * (x1 - x0);
			};

		float sx0 = mapX(safeMin);
		float sx1 = mapX(safeMax);

		// 1) crveno lijevo
		pushRect(x0, sx0, y0, y1, 1.0f, 0.0f, 0.0f);
		// 2) zeleno sredina
		pushRect(sx0, sx1, y0, y1, 0.0f, 1.0f, 0.0f);
		// 3) crveno desno
		pushRect(sx1, x1, y0, y1, 1.0f, 0.0f, 0.0f);

		// 4) okvir (tanki bijeli rub) — kao 4 tanka recta
		float t = 0.005f;
		pushRect(x0, x1, y1 - t, y1, 1.0f, 1.0f, 1.0f); // top
		pushRect(x0, x1, y0, y0 + t, 1.0f, 1.0f, 1.0f); // bottom
		pushRect(x0, x0 + t, y0, y1, 1.0f, 1.0f, 1.0f); // left
		pushRect(x1 - t, x1, y0, y1, 1.0f, 1.0f, 1.0f); // right

		// 5) kazaljka (bijela vertikalna linija kao tanki pravokutnik)
		float px = mapX(value);
		float lw = 0.004f;
		pushRect(px - lw, px + lw, y0 - 0.02f, y1 + 0.02f, 1.0f, 1.0f, 1.0f);
	}
};

static bool rayPlaneIntersect(
	const glm::vec3& rayOrigin,
	const glm::vec3& rayDir,
	const glm::vec3& planePoint,
	const glm::vec3& planeNormal,
	glm::vec3& outHit)
{
	float denom = glm::dot(planeNormal, rayDir);
	if (fabs(denom) < 1e-6f) return false; // paralelno

	float t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
	if (t < 0.0f) return false; // iza kamere

	outHit = rayOrigin + rayDir * t;
	return true;
}

static glm::vec3 screenToWorldRay(
	double mx, double my,
	int w, int h,
	const glm::mat4& view,
	const glm::mat4& proj)
{
	// NDC [-1,1]
	float x = (2.0f * (float)mx) / (float)w - 1.0f;
	float y = 1.0f - (2.0f * (float)my) / (float)h;

	glm::vec4 rayClip(x, y, -1.0f, 1.0f);
	glm::vec4 rayEye = glm::inverse(proj) * rayClip;
	rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

	glm::vec3 rayWorld = glm::normalize(glm::vec3(glm::inverse(view) * rayEye));
	return rayWorld;
}

bool getMouseClickOnBoard(
	GLFWwindow* window,
	int width, int height,
	const glm::mat4& view,
	const glm::mat4& proj,
	const DartBoard& board,
	glm::vec3& outBoardLocalPos)
{
	// 1) uzmi mouse position
	double mx, my;
	glfwGetCursorPos(window, &mx, &my);

	// 2) kamera pozicija
	glm::vec3 camPos = glm::vec3(glm::inverse(view)[3]);

	// 3) ray iz kamere
	glm::vec3 rayDir = screenToWorldRay(mx, my, width, height, view, proj);

	// 4) ravnina ploče (u WORLD prostoru)
	glm::mat4 boardModel = board.boardModel;
	glm::vec3 planePoint = glm::vec3(boardModel[3]);         // centar ploče
	glm::vec3 planeNormal = glm::normalize(glm::vec3(boardModel * glm::vec4(0, 0, 1, 0)));

	glm::vec3 hitWorld;
	if (!rayPlaneIntersect(camPos, rayDir, planePoint, planeNormal, hitWorld))
		return false;

	// 5) WORLD → BOARD LOCAL
	outBoardLocalPos = board.worldToBoard(hitWorld);
	return true;
}

class BackdropWall {
public:
	Shader* shader = nullptr;

	GLuint VAO = 0;
	GLuint VBO = 0;
	GLuint EBO = 0;
	GLuint tex = 0;

	GLint model_loc = -1;
	GLint view_loc = -1;
	GLint projection_loc = -1;
	GLint tex_loc = -1;

	glm::mat4 model = glm::mat4(1.0f);

	BackdropWall(Shader* sh)
	{
		shader = sh;

		// quad: pos3 + uv2
		float vertices[] = {
			// pos              // uv
			-1.f, -1.f, 0.f,     0.f, 0.f,
			 1.f, -1.f, 0.f,     1.f, 0.f,
			 1.f,  1.f, 0.f,     1.f, 1.f,
			-1.f,  1.f, 0.f,     0.f, 1.f
		};

		unsigned int indices[] = {
			0, 1, 2,
			2, 3, 0
		};

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

		// pos
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// uv
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);

		glBindVertexArray(0);
	}

	void setModel(const glm::mat4& m) {
		model = m;
	}

	bool loadTexture(const std::string& path)
	{
		tex = loadTextureFromFile(path);
		return tex != 0;
	}

	void reload()
	{
		model_loc = glGetUniformLocation(shader->ID, "model");
		view_loc = glGetUniformLocation(shader->ID, "view");
		projection_loc = glGetUniformLocation(shader->ID, "project");
		tex_loc = glGetUniformLocation(shader->ID, "uTex");
	}

	void draw(const glm::mat4& view, const glm::mat4& proj)
	{
		glUseProgram(shader->ID);

		glUniformMatrix4fv(model_loc, 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(view_loc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projection_loc, 1, GL_FALSE, glm::value_ptr(proj));

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform1i(tex_loc, 0);

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

	~BackdropWall()
	{
		if (tex) glDeleteTextures(1, &tex);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		glDeleteVertexArrays(1, &VAO);
	}
};

int main(int argc, char* argv[]) {

	std::cout << "Welcome to the game of Darts!\nThis is the game for 2 people where you start from 501.\n"
		<< "The rules are simple, first one player has 3 darts, then the second one and so on.\n"
		<< "We play the darts by clicking with left mouse button to our target"
		<< "\nand then we need to as precisely as possible hit the indicator in the middle.\n"
		<< "Enjoy!\n\nFirst is Player1!\n";

	load_fbx(argv[0], "dartboard.obj");
	load_dart_object(argv[0], "dart.obj");

	GLFWwindow* window;

	glfwInit();

	window = glfwCreateWindow(width, height, "neindeksirano/indeksirano/instancirano", nullptr, nullptr);

	if (window == nullptr) {
		fprintf(stderr, "Failed to Create OpenGL Context");
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);

	GLFWmonitor* monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = glfwGetVideoMode(monitor);

	int winW = width; 
	int winH = height;

	int x = 20;
	int y = (mode->height - winH) / 2;

	glfwSetWindowPos(window, x, y);


	gladLoadGL();

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);


	glClearColor(0, 0, 0, 1);

	Shader* shader = loadShader(argv[0], "shader");
	Shader* shader0 = loadShader(argv[0], "shader0");
	Shader* shader1 = loadShader(argv[0], "shader1");
	Shader* shaderDart = loadShader(argv[0], "shader1");
	Shader* shaderAim = loadShader(argv[0], "aimbar");

	DartBoard board(shader0, 192);
	Dart dart(shaderDart);
	TexturedDartboard visual(shader1);
	AimBar aim(shaderAim);
	BackdropWall wall(shader1);   // isti shader kao dartboard

	std::string texPath = getResourcesDir(argv[0]) + "wood_wall.jpg";

	wall.loadTexture(texPath);
	wall.reload();


	glEnable(GL_DEPTH_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	aim.setSafeZone(-0.3f, 0.3f);
	aim.reload();
	board.reload();
	visual.buildFromGlobals();
	dart.buildFromGlobals();

	// postavljanje ploce s teksturom

	glm::mat4 m(1.0f);

	float s = 1.28f * 1.23f;
	m = glm::scale(m, glm::vec3(s));

	m = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1, 0, 0)) * m;

	m = glm::translate(glm::mat4(1.0f), glm::vec3(-0.043f, 1.91f, -1.31f)) * m;

	visual.setModel(m);

	glm::mat4 view = board.poligon.getViewMatrix();
	glm::mat4 proj = board.frustum;

	// postavljanje pocetne pozicije strelice

	dart.setRotationEulerDeg(glm::vec3(45.0f, 0.0f, 0.0f));
	dart.setScale(glm::vec3(0.1f, 0.1f, 0.1f));
	dart.setPosition(glm::vec3(0.0f, -0.5f, 4.0f));

	glm::mat4 wallModel(1.0f);

	// skaliraj da bude veći od ploče
	wallModel = glm::scale(wallModel, glm::vec3(6.0f, 6.0f, 1.0f));

	// pomakni iza dartboarda (Z iza)
	wallModel = glm::translate(glm::mat4(1.0f),
		glm::vec3(-0.043f, 1.91f, -2.5f)) * wallModel;

	wall.setModel(wallModel);


	// postavljanje varijabli za animaciju strelice

	glm::vec3 P0 = glm::vec3(0.0f, -0.5f, 4.0f);   // start
	glm::vec3 P3 = glm::vec3(1.15f, 0.0f, -1.31f); // pozicija ploce

	float arcHeight = 1.5f; // koliko strelica ide gore
	glm::vec3 up(0, 1, 0);

	// kontrolne točke (parabola)
	glm::vec3 P1 = P0 + up * arcHeight + glm::vec3(0, 0, -1.0f);
	glm::vec3 P2 = P3 + up * arcHeight + glm::vec3(0, 0, 1.0f);

	float t = 0.0f;
	float speed = 0.0f;    // brzina
	float tipOffset = 0.35f; // offset vrha strelice


	double lastTime = glfwGetTime();

	int score_player1 = 501;
	int score_player2 = 501;

	int turn = 0;
	int darts = 0;
	int hitScore = 0;

	bool aimRunning = false;
	bool shotReady = false;

	float aim_x = 0.0f;
	float aim_y = 0.0f;
	float aim_speed = 0.001f;

	float maxError = 0.1f;
	glm::vec3 boardLocal;

	//glavna petlja programa
	while (glfwWindowShouldClose(window) == false) {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		double now = glfwGetTime();
		float dt = (float)(now - lastTime);
		lastTime = now;

		static bool prevClick = false;
		bool clickNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

		if (clickNow && !prevClick)
		{
			darts++;

			int recieveScore = 0;

			if (getMouseClickOnBoard(window, width, height, view, proj, board, boardLocal))
			{
				aimRunning = true;
			}
		}
		prevClick = clickNow;

		if (aimRunning) 
		{
			static bool prevSpace = false;
			bool space = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

			if (space && !prevSpace)
			{
				aimRunning = false;
				
				int recieveScore = 0;

				float tmp_ax = aim_x;
				float tmp_ay = aim_y;

				if (tmp_ax >= -0.03 && tmp_ax <= 0.03)
				{
					tmp_ax = 0;
				}
				if (tmp_ay >= -0.03 && tmp_ay <= 0.03)
				{
					tmp_ay = 0;
				}

				boardLocal += glm::vec3(tmp_ax, tmp_ay, 0);

				//std::cout << "Greška kod pogađanja naciljane točke je: " << aim_x << "\n";

				recieveScore = board.scoreAtLocal(boardLocal);
				//std::cout << "Vrijednost nakon offseta bila bi: " << recieveScore << "\n";

				hitScore += recieveScore;

				P3 = boardLocal;
				P1 = glm::vec3((int)((P0.x + P3.x) / 3), (int)((P0.y + P3.y) / 3), (int)((P0.z + P3.z) / 2 + P0.z));
				P2 = glm::vec3((int)(2 * (P0.x + P3.x) / 3), (int)(2 * (P0.y + P3.y) / 3), (int)((P0.z + P3.z) / 2 + P0.z));

				t = 0.0f;
				speed = 0.6f;   // ali neka je speed VANJSKI, ne float lokalni!


				if (turn == 0)
				{
					if (score_player1 - hitScore == 0)
					{
						printf("\nPlayer1 WINS!\n\nNext game starts Player2! \n\n");
						darts = 0;
						turn = 1;
						hitScore = 0;
						score_player1 = 501;
						score_player2 = 501;
						continue;
					}
					else if (score_player1 - hitScore < 0)
					{
						printf("\nPlayer1 Bust!\n\n");
						darts = 0;
						turn = 1;
						hitScore = 0;
						continue;
					}

					std::cout << "Player1 scored scored: " << recieveScore << "     Player1: " << score_player1 - hitScore << "| Player2: "
						<< score_player2 << "\n";

					if (darts == 3)
					{
						score_player1 = score_player1 - hitScore;
						darts = 0;
						turn = 1;
						hitScore = 0;
						printf("\nNow is Player2 turn!\n");
					}
				}
				else if (turn == 1)
				{
					if (score_player2 - hitScore == 0)
					{
						printf("\nPlayer2 WINS!\n\nNext game starts Player1! \n\n");
						darts = 0;
						turn = 0;
						hitScore = 0;
						score_player1 = 501;
						score_player2 = 501;
						continue;
					}
					else if (score_player2 - hitScore < 0)
					{
						printf("\nPlayer2 Bust!\n\n");
						darts = 0;
						turn = 0;
						hitScore = 0;
						continue;
					}

					std::cout << "Player2 scored scored: " << recieveScore << "     Player1: " << score_player1 << "| Player2: "
						<< score_player2 - hitScore << "\n";

					if (darts == 3)
					{
						score_player2 = score_player2 - hitScore;
						darts = 0;
						turn = 0;
						hitScore = 0;
						printf("\nNow is Player1 turn!\n");
					}

				}
			}
			prevSpace = space;
		}

		glm::mat4 dartModel = dart.getModel();
		glm::mat4 dartBoardModel = visual.model;

		glm::vec3 currPos = glm::vec3(dartModel[3]);
		glm::vec3 dartBoardPos = glm::vec3(dartBoardModel[3]);

		// update parametra t
		t += dt * speed;
		if (t > 1.0f) t = 1.0f;

		// pozicija na paraboli
		glm::vec3 pos = bezier3(P0, P1, P2, P3, t);

		// tangenta
		glm::vec3 tan = bezier3_tangent(P0, P1, P2, P3, t);
		if (glm::length(tan) < 1e-6f)
		{
			tan = glm::vec3(0, 0, -1);
		}

		tan = glm::normalize(tan);

		// želimo rotaciju koja mapira "forward" osi strelice na tangentu
		// ako je strelica forward = -Z:
		glm::vec3 forward0 = glm::vec3(0, 0, -1);

		// os rotacije i kut
		glm::vec3 axis = glm::cross(forward0, tan);
		float axisLen = glm::length(axis);

		float angle = 0.0f;
		if (axisLen < 1e-6f) {
			// paralelno ili antiparalelno
			float d = glm::clamp(glm::dot(forward0, tan), -1.0f, 1.0f);
			if (d < 0.0f) {
				axis = glm::vec3(0, 1, 0);
				angle = glm::pi<float>();
			}
			else {
				axis = glm::vec3(0, 1, 0);
				angle = 0.0f;
			}
		}
		else {
			axis /= axisLen;
			angle = acos(glm::clamp(glm::dot(forward0, tan), -1.0f, 1.0f));
		}

		glm::quat q = glm::angleAxis(angle, glm::normalize(axis));

		// 3) Quaternion -> Euler (u radijanima)
		glm::vec3 eulerRad = glm::eulerAngles(q);

		// 4) Radijani -> stupnjevi
		glm::vec3 eulerDeg = glm::degrees(eulerRad);

		dart.setRotationEulerDeg(eulerDeg);
		dart.setPosition(pos);

		// pogodak i zaustavi
		if (t >= 1.0f) {
			speed = 0;
		}

		aim_x += aim_speed;
		aim_y += aim_speed;

		if (aim_x >= maxError)
		{
			aim_x = maxError;
			aim_y = maxError;
			aim_speed = 0 - aim_speed;
		}
		else if (aim_x <= 0 - maxError)
		{
			aim_x = 0 - maxError;
			aim_y = 0 - maxError;
			aim_speed = 0 - aim_speed;
		}
		
		//std::cout << "\naimx aimy" << aim_x << " " << aim_y << "\n";

		wall.draw(view, proj);
		aim.setValue(aim_x / maxError);
		aim.draw();
		board.draw();
		visual.draw(view, proj);
		dart.draw(view, proj);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}


	glfwTerminate();


	return EXIT_SUCCESS;
}
