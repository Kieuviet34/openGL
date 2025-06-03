#include "lodterrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include "../lib/stb_image.h"
#include <future>
#include <random>
#include <iostream>
#include <cmath>

//helper function
float LodTerrain::getHeightAt(float worldX, float worldZ) const {
    // we only have one tile in this version
    const auto& tile = _tiles[0];

    // map from world coords into [0..1] over the tile’s XZ‐extent
    float u = (worldX - tile.origin.x) / _scale;
    float v = (worldZ - tile.origin.z) / _scale;
    u = glm::clamp(u, 0.0f, 1.0f);
    v = glm::clamp(v, 0.0f, 1.0f);

    // now map into heightmap grid coordinates
    float gx = u * (tile.gridSize - 1);
    float gz = v * (tile.gridSize - 1);
    int x0 = int(floor(gx)), z0 = int(floor(gz));
    int x1 = std::min(x0+1, int(tile.gridSize-1));
    int z1 = std::min(z0+1, int(tile.gridSize-1));
    float sx = gx - x0, sz = gz - z0;

    // fetch the four corner heights
    const auto& H = tile.heightmap;
    float h00 = H[z0 * tile.gridSize + x0];
    float h10 = H[z0 * tile.gridSize + x1];
    float h01 = H[z1 * tile.gridSize + x0];
    float h11 = H[z1 * tile.gridSize + x1];

    // bilinear interpolation
    float a = glm::mix(h00, h10, sx);
    float b = glm::mix(h01, h11, sx);
    float height = glm::mix(a, b, sz);

    // add the base‐level Y offset
    return height + tile.origin.y;
}

glm::vec3 LodTerrain::getNormalAt(float worldX, float worldZ) const {
    // small delta in world units for derivative
    float d = _scale / float(_tileSize * 10);
    // sample neighbors
    float hl = getHeightAt(worldX - d, worldZ);
    float hr = getHeightAt(worldX + d, worldZ);
    float hd = getHeightAt(worldX, worldZ - d);
    float hu = getHeightAt(worldX, worldZ + d);

    // build tangent & bitangent vectors
    glm::vec3 tan{ 2*d, hr - hl, 0.0f };
    glm::vec3 bit{ 0.0f, hu - hd, 2*d };
    return glm::normalize(glm::cross(tan, bit));
}


// diamond–square helper
static float getH(float s, std::size_t d, std::mt19937& g) {
    std::uniform_real_distribution<> dis(-1.0,1.0);
    float sign = dis(g)>0 ? 1.0f : -1.0f;
    float r = std::pow(2.0f, -s * float(d));
    return sign * std::abs(dis(g)) * r;
}

// generate full (2^k+1)^2 fractal
static std::vector<std::vector<float>> generateTerrain(std::size_t size, float smoothness){
    if(size&(size-1)) throw std::invalid_argument("size must be power of two");
    std::size_t N = size+1;
    std::vector<std::vector<float>> m(N, std::vector<float>(N));
    std::mt19937 gen(1337);
    std::uniform_real_distribution<> dis(-1,1);

    m[0][0]=dis(gen); m[0][size]=dis(gen);
    m[size][0]=dis(gen); m[size][size]=dis(gen);

    int steps = std::log2(size);
    for(int d=1; d<=steps; ++d){
        std::size_t step = size>>(d-1), half=step>>1;
        // diamond
        for(std::size_t y=0; y<size; y+=step){
            for(std::size_t x=0; x<size; x+=step){
                float avg = (m[y][x]+m[y][x+step]+m[y+step][x]+m[y+step][x+step])*0.25f;
                m[y+half][x+half] = avg + getH(smoothness,d,gen);
            }
        }
        // square
        for(std::size_t y=0; y<=size; y+=half){
            for(std::size_t x=(y+half)%step; x<=size; x+=step){
                float sum=0; int cnt=0;
                if(x>=half){sum+=m[y][x-half];++cnt;}
                if(x+half<=size){sum+=m[y][x+half];++cnt;}
                if(y>=half){sum+=m[y-half][x];++cnt;}
                if(y+half<=size){sum+=m[y+half][x];++cnt;}
                m[y][x] = sum/cnt + getH(smoothness,d,gen);
            }
        }
    }
    return m;
}

// find smallest 2^k>=n
static std::size_t smallestPow2(std::size_t n){
    std::size_t p=1;
    while(p<n) p<<=1;
    return p;
}

LodTerrain::LodTerrain(std::size_t tileSize,
                       int lodLevels,
                       float scale,
                       float heightScale,
                       float smoothness,
                       const std::string& a,
                       const std::string& n)
  : _lodLevels(lodLevels)
  , _tileSize(tileSize)
  , _scale(scale)
  , _heightScale(heightScale)
  , _smoothness(smoothness)
  , _yOffset(0.0f)
{
    initializeNoise();
    initializeTextures(a,n);
    initializeTiles();
}

LodTerrain::~LodTerrain(){
    // free GPU
    for(auto& t:_tiles){
        for(auto& L:t.lods){
            glDeleteVertexArrays(1,&L.vao);
            glDeleteBuffers(1,&L.vbo);
            glDeleteBuffers(1,&L.ebo);
        }
    }
    glDeleteTextures(1,&_albedo);
    glDeleteTextures(1,&_normal);
}

void LodTerrain::initializeNoise(){
    _detailNoise.SetNoiseType(FastNoiseLite::NoiseType_Cellular);
    _detailNoise.SetFrequency(0.2f);
}

void LodTerrain::loadTexture(const std::string& path, GLuint& texID){
    int w,h,c;
    unsigned char* data=stbi_load(path.c_str(),&w,&h,&c,0);
    if(!data){ std::cerr<<"Failed to load "<<path<<"\n"; return; }
    GLenum fmt=(c==3?GL_RGB:GL_RGBA);
    glGenTextures(1,&texID);
    glBindTexture(GL_TEXTURE_2D,texID);
    glTexImage2D(GL_TEXTURE_2D,0,fmt,w,h,0,fmt,GL_UNSIGNED_BYTE,data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    stbi_image_free(data);
}

void LodTerrain::initializeTextures(const std::string& a,const std::string& n){
    loadTexture(a,_albedo);
    loadTexture(n,_normal);
}

void LodTerrain::initializeTiles(){
    Tile base;
    base.origin = glm::vec3(-_scale*0.5f, _yOffset, -_scale*0.5f);
    std::size_t N = smallestPow2(_tileSize);
    base.gridSize = N+1;

    auto raw = generateTerrain(N,_smoothness);
    base.heightmap.resize((N+1)*(N+1));
    for(std::size_t z=0;z<=N;++z)
      for(std::size_t x=0;x<=N;++x)
        base.heightmap[z*(N+1)+x] = raw[z][x]*_heightScale;

    _tiles.push_back(std::move(base));
    generateLODs(_tiles[0]);
}

void LodTerrain::generateLODs(Tile& tile){
    // we only have one tile, generate each LOD
    for(int lvl=0;lvl<_lodLevels;++lvl){
        std::size_t res = std::max<std::size_t>(_tileSize>>lvl,2u);
        TileLOD lod{};
        buildTileMesh(tile,lod,res);
        // center for LOD distance check:
        lod.center = tile.origin + glm::vec3(_scale*0.5f,0,_scale*0.5f);
        tile.lods.push_back(lod);
    }
}

void LodTerrain::buildTileMesh(Tile& tile, TileLOD& lod, std::size_t res){
    // gather positions, uvs, normals
    std::size_t N2 = tile.gridSize-1;
    float step = _scale/float(res-1);

    struct V{ glm::vec3 p; glm::vec2 uv; glm::vec3 n; };
    std::vector<V> verts; verts.reserve(res*res);

    for(std::size_t z=0;z<res;++z){
      for(std::size_t x=0;x<res;++x){
        float u=float(x)/(res-1), v=float(z)/(res-1);
        std::size_t i=std::min<std::size_t>(std::round(u*N2),N2),
                     j=std::min<std::size_t>(std::round(v*N2),N2);
        glm::vec3 P = tile.origin +
            glm::vec3(x*step,
                      tile.heightmap[j*(N2+1)+i],
                      z*step);
        verts.push_back({P, {u,u}, {0,1,0}}); // normal will fix below
      }
    }

    // compute normals via finite‐difference
    auto idx=[&](int x,int z){ return z*res + x; };
    for(int z=0;z<res;++z){
      for(int x=0;x<res;++x){
        float hl=verts[idx(std::max(x-1,0),z)].p.y;
        float hr=verts[idx(std::min(int(x+1),int(res-1)),z)].p.y;
        float hd=verts[idx(x,std::max(z-1,0))].p.y;
        float hu=verts[idx(x,std::min(int(z+1),int(res-1)))].p.y;
        glm::vec3 tan{2*step, hr-hl,0}, bit{0,hu-hd,2*step};
        verts[idx(x,z)].n = glm::normalize(glm::cross(tan,bit));
      }
    }

    // build index array
    std::vector<GLuint> idxs; idxs.reserve((res-1)*(res-1)*6);
    for(std::size_t z=0;z+1<res;++z){
      for(std::size_t x=0;x+1<res;++x){
        GLuint tl=z*res+x, tr=tl+1, bl=tl+res, br=bl+1;
        idxs.insert(idxs.end(),{tl,bl,tr,tr,bl,br});
      }
    }
    lod.indexCount = idxs.size();

    // upload to GL
    glGenVertexArrays(1,&lod.vao);
    glGenBuffers(1,&lod.vbo);
    glGenBuffers(1,&lod.ebo);

    glBindVertexArray(lod.vao);
      glBindBuffer(GL_ARRAY_BUFFER,lod.vbo);
      glBufferData(GL_ARRAY_BUFFER,verts.size()*sizeof(V),verts.data(),GL_STATIC_DRAW);

      glEnableVertexAttribArray(0);
      glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)0);
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,uv));
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(V),(void*)offsetof(V,n));

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,lod.ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,idxs.size()*sizeof(GLuint),idxs.data(),GL_STATIC_DRAW);
    glBindVertexArray(0);
}

void LodTerrain::Draw(const glm::vec3& camPos){
    // bind textures to unit 0/1
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D,_albedo);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D,_normal);

    // choose LOD by distance
    static const float dists[] = {50,100,200,400};
    float dist = glm::distance(camPos,_tiles[0].lods[0].center);
    int  lod  = 0;
    while(lod+1<_lodLevels && dist>dists[lod]) ++lod;

    TileLOD const& L = _tiles[0].lods[lod];
    glBindVertexArray(L.vao);
    glDrawElements(GL_TRIANGLES,L.indexCount,GL_UNSIGNED_INT,nullptr);
    glBindVertexArray(0);
}
