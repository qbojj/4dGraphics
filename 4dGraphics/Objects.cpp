#include "stdafx.h"
#include "Objects.h"
using namespace std;
/*
Pixel PixelF(float red, float green, float blue, float alpha)
{
	return Pixel(uint8_t(red * 255.0f), uint8_t(green * 255.0f), uint8_t(blue * 255.0f), uint8_t(alpha * 255.0f));
}

Pixel PixelLerp(Pixel p1, Pixel p2, float t)
{
	return (p2 * t) + p1 * (1.f - t);
}

Pixel  Pixel::operator * (const float i) const
{
	float fR = min(255.0f, max(0.0f, float(r) * i));
	float fG = min(255.0f, max(0.0f, float(g) * i));
	float fB = min(255.0f, max(0.0f, float(b) * i));
	return Pixel(uint8_t(fR), uint8_t(fG), uint8_t(fB), a);
}

Pixel  Pixel::operator / (const float i) const
{
	float fR = min(255.0f, max(0.0f, float(r) / i));
	float fG = min(255.0f, max(0.0f, float(g) / i));
	float fB = min(255.0f, max(0.0f, float(b) / i));
	return Pixel(uint8_t(fR), uint8_t(fG), uint8_t(fB), a);
}

Pixel& Pixel::operator *=(const float i)
{
	this->r = uint8_t(min(255.0f, max(0.0f, float(r) * i)));
	this->g = uint8_t(min(255.0f, max(0.0f, float(g) * i)));
	this->b = uint8_t(min(255.0f, max(0.0f, float(b) * i)));
	return *this;
}

Pixel& Pixel::operator /=(const float i)
{
	this->r = uint8_t(min(255.0f, max(0.0f, float(r) / i)));
	this->g = uint8_t(min(255.0f, max(0.0f, float(g) / i)));
	this->b = uint8_t(min(255.0f, max(0.0f, float(b) / i)));
	return *this;
}

Pixel  Pixel::operator + (const Pixel& p) const
{
	uint8_t nR = uint8_t(min(255, max(0, int(r) + int(p.r))));
	uint8_t nG = uint8_t(min(255, max(0, int(g) + int(p.g))));
	uint8_t nB = uint8_t(min(255, max(0, int(b) + int(p.b))));
	return Pixel(nR, nG, nB, a);
}

Pixel  Pixel::operator - (const Pixel& p) const
{
	uint8_t nR = uint8_t(min(255, max(0, int(r) - int(p.r))));
	uint8_t nG = uint8_t(min(255, max(0, int(g) - int(p.g))));
	uint8_t nB = uint8_t(min(255, max(0, int(b) - int(p.b))));
	return Pixel(nR, nG, nB, a);
}

Pixel& Pixel::operator += (const Pixel& p)
{
	this->r = uint8_t(min(255, max(0, int(r) + int(p.r))));
	this->g = uint8_t(min(255, max(0, int(g) + int(p.g))));
	this->b = uint8_t(min(255, max(0, int(b) + int(p.b))));
	return *this;
}

Pixel& Pixel::operator -= (const Pixel& p)
{
	this->r = uint8_t(min(255, max(0, int(r) + int(p.r))));
	this->g = uint8_t(min(255, max(0, int(g) + int(p.g))));
	this->b = uint8_t(min(255, max(0, int(b) + int(p.b))));
	return *this;
}
*/

/*
struct NamedMaterial
{
	GLMaterialSpec mat;
	std::string name;
};

static pair< vector<NamedMaterial>, vector<string> > LoadMTL( const Path &file )
{
	stringstream f;
	{
		ifstream _f( file );
		if( !f ) return {};
		f << _f.rdbuf();
	}

	std::filesystem::path pth = file.data();
	pth = pth.parent_path();

	vector<NamedMaterial> mats;
	vector<string> textures;

	robin_hood::unordered_map<string, int> NameToTextureNum;

	auto CreateTexture = [&]( const string_view &s ) -> void
	{
		NameToTextureNum[s.data()] = (int)textures.size();
		textures.emplace_back( ( pth / s ).u8string().c_str() );
	};

	string s;
	while( getline( f, s ) )
	{
		if( s.length() == 0 || s[0] == '#' ) continue;

		glm::vec3 color;
		float fVal;
		int iVal;

		int sq = (int)s.find_first_of( " \t", 0 );

		string_view cmd = s;
		cmd = cmd.substr( 0, sq );

		string_view data = s;
		data = data.substr( data.find_first_not_of( " \t", sq ) );

		string_view last = data.substr( data.find_first_not_of( " \t" ) );

		if( cmd == "newmtl" )
		{
			NamedMaterial m;
			m.name = last;
			mats.push_back( move( m ) );
		}
		else if( cmd == "Ka" )
		{
			sscanf( data.data(), "%f %f %f", &color.x, &color.y, &color.z );
			mats.back().mat.cAmbient = color;
		}
		else if( cmd == "Kd" )
		{
			sscanf( data.data(), "%f %f %f", &color.x, &color.y, &color.z );
			mats.back().mat.cDiffuse = color;
		}
		else if( cmd == "Ks" )
		{
			sscanf( data.data(), "%f %f %f", &color.x, &color.y, &color.z );
			mats.back().mat.cSpecular = color;
		}
		else if( cmd == "d" )
		{
			sscanf( data.data(), "%f", &fVal );
			mats.back().mat.fAlpha = fVal;
		}
		else if( cmd == "Tr" )
		{
			sscanf( data.data(), "%f", &fVal );
			mats.back().mat.fAlpha = 1.f - fVal;
		}
		else if( cmd == "Ns" )
		{
			sscanf( data.data(), "%f", &fVal );
			mats.back().mat.fShininess = fVal;
		}
		else if( cmd == "illum" )
		{
			sscanf( data.data(), "%d", &iVal );
			mats.back().mat.iIllum = iVal;
		}
		else if( cmd == "map_Kd" )
		{
			if( NameToTextureNum.find( last.data() ) == NameToTextureNum.end() ) CreateTexture( last );

			mats.back().mat.tTex = NameToTextureNum[last.data()];
		}
	}

	return make_pair( move(mats), move(textures) );
}

mesh_t LoadOBJ( const Path &file, bool reverseFaces )
{
	stringstream f;

	{
		ifstream _f( file );
		if( !_f ) return {};
		f << _f.rdbuf();
		_f.close();
	}

	robin_hood::unordered_map<string, int> MatNameToPos;
	robin_hood::unordered_map<string, int> TexNameToPos;
	mesh_t mesh;

	mesh.vert.reserve( 1 << 12 );
	mesh.tex.reserve( 1 << 12 );
	mesh.norm.reserve( 1 << 12 );

	mesh.textures.push_back( "NoTexture.jpg" );
	TexNameToPos["NoTexture.jpg"] = 0;
	//mesh.textures[0].MakeDefault();

	// create default material
	mesh.mat.push_back( {} );
	mesh.mat[0].tTex = 0;
	mesh.tex.push_back( { 0,0,0 } );

	filesystem::path pth( file.data() );

	mesh.MeshName = pth.filename().string();

	pth = pth.parent_path();

	string s;

	int currMaterial = 0;

	while (getline(f, s))
	{
		if (s.length() == 0 || s[0] == '#') continue;

		if( s[0] == 'm' )
		{
			// mtllib
			string_view q = s;
			q = q.substr( q.find_first_not_of( " \t", strlen( "mtllib" ) ) );

			int txSt = (int)mesh.textures.size();

			auto a = LoadMTL( ( pth / q ).u8string().c_str() );

			for( auto &i : a.second )
			{
				if( TexNameToPos.find( i ) != TexNameToPos.end() ) continue;

				TexNameToPos[i] = (int)mesh.textures.size();
				mesh.textures.push_back( i );
			}

			for( auto &i : a.first )
			{
				MatNameToPos[ i.name ] = (int)mesh.mat.size();

				if( i.mat.tTex != -1 ) i.mat.tTex = TexNameToPos[ a.second[i.mat.tTex] ];
				else i.mat.tTex = 0;

				mesh.mat.push_back( move( i.mat ) );
			}
		}
		else if( s[0] == 'u' )
		{
			// usemtl
			string_view q = s;
			q = q.substr( q.find_first_not_of( " \t", strlen( "usemtl" ) ) );

			currMaterial = MatNameToPos[ q.data() ];
		}
		else if (s[0] == 'v')
		{
			glm::vec4 v4;
			int cnt = sscanf(s.c_str(), "%*s %f %f %f %f", &v4.x, &v4.y, &v4.z, &v4.w);

			switch (s.at(1))
			{
			case ' ': 
				if( cnt < 3 ) return {};
				if( cnt == 3 ) v4.w = 1.f;
				mesh.vert.push_back(v4);
				break;
			case 't': 
				if( cnt < 1 ) return {};
				mesh.tex.push_back( { v4.x, cnt > 1 ? v4.y : 0, cnt > 2 ? v4.z : 0 } );
				break;
			case 'n': 
				if( cnt != 3 ) return {};
				mesh.norm.push_back( glm::normalize( glm::vec3{ v4.x, v4.y, v4.z } ) );
				break;
			default: // unsupported 'vp' (free form geometry)  
				break;
			}
		}
		else if (s[0] == 'f')
		{
			int st = (int)s.find_first_of("0123456789.", 1);

			int p[2] = { 0, 0 }, cnt = 0;

			int i = st;
			for (int i = st; (s[i] >= '0' && s[i] <= '9') || s[i] == '/'; i++)
				if (s[i] == '/')
					p[cnt++] = i;

			int fV, fT, fN; // first vertice, texture, normal
			int sV, sT, sN; // second ...
			int cV, cT, cN; // current ...

			const char* S = s.c_str(); int n;
			mesh_t::face t;

			
			auto SetNorm = [&](mesh_t::face& t)
			{
				glm::vec3 v0 = glm::vec3( mesh.vert[t.vert[0].vert] ) / mesh.vert[t.vert[0].vert].w;
				glm::vec3 v1 = glm::vec3( mesh.vert[t.vert[1].vert] ) / mesh.vert[t.vert[1].vert].w;
				glm::vec3 v2 = glm::vec3( mesh.vert[t.vert[2].vert] ) / mesh.vert[t.vert[2].vert].w;

				glm::vec3 d1 = ( v1 - v0 );
				glm::vec3 d2 = ( v2 - v0 );

				glm::vec3 norm = glm::normalize( glm::cross( d1, d2 ) );

				for( int i = 0; i < t.vert.size(); i++ ) t.vert[i].norm = (int)mesh.norm.size();
				mesh.norm.push_back(norm);
			};
			

			if (cnt == 0)
			{
				//v: v

				(void)sscanf(S, "%*s %d %d %n", &fV, &sV, &n); S += n;

				t.material = currMaterial;
				t.vert.push_back( { fV - 1, -1, 0 } );
				t.vert.push_back( { sV - 1, -1, 0 } );

				while (S[0] != '\0')
				{
					(void)sscanf(S, "%d %n", &cV, &n); S += n;
					t.vert.push_back( { cV - 1, -1, 0 } );
				}

				SetNorm( t );
				mesh.faces.push_back( t );
			}
			else if (cnt == 1)
			{
				//v: v/vt

				(void)sscanf(S, "%*s %d/%d %d/%d%n", &fV, &fT, &sV, &sT, &n); S += n;

				t.material = currMaterial;
				t.vert.push_back( { fV - 1, -1, fT } );
				t.vert.push_back( { sV - 1, -1, sT } );

				while (S[0] != '\0')
				{
					(void)sscanf(S, "%d/%d %n", &cV, &cT, &n); S += n;
					t.vert.push_back( { cV - 1, -1, cT } );
				}

				SetNorm( t );
				mesh.faces.push_back( t );
			}
			else if (cnt == 2)
			{
				if (p[1] == p[0] + 1)
				{
					//v: v//vn

					(void)sscanf(S, "%*s %d//%d %d//%d %n", &fV, &fN, &sV, &sN, &n); S += n;

					t.material = currMaterial;
					t.vert.push_back( { fV - 1, fN - 1, 0 } );
					t.vert.push_back( { sV - 1, sN - 1, 0 } );

					while (S[0] != '\0')
					{
						(void)sscanf(S, "%d//%d %n", &cV, &cN, &n); S += n;
						t.vert.push_back( { cV - 1, cN - 1, 0 } );
					}
					mesh.faces.push_back( t );
				}
				else
				{
					//v: v/vt/vn

					(void)sscanf(S, "%*s %d/%d/%d %d/%d/%d %n", &fV, &fT, &fN, &sV, &sT, &sN, &n); S += n;

					t.material = currMaterial;
					t.vert.push_back( { fV - 1, fN - 1, fT } );
					t.vert.push_back( { sV - 1, sN - 1, sT } );

					while (S[0] != '\0')
					{
						(void)sscanf(S, "%d/%d/%d %n", &cV, &cT, &cN, &n); S += n;
						t.vert.push_back( { cV - 1, cN - 1, cT } );
					}

					mesh.faces.push_back( t );
				}
			}

		}
	}

	if( reverseFaces )
	{
		for( auto &a : mesh.faces ) reverse( a.vert.begin(), a.vert.end() );
		for( auto &a : mesh.norm ) a = -a;
	}

	mesh.vert.shrink_to_fit();
	mesh.tex.shrink_to_fit();
	mesh.norm.shrink_to_fit();

	{
		vector<bool> Used( mesh.mat.size() );
		for( auto &a : mesh.faces ) Used[a.material] = true;

		bool AnyUnused = false;
		for( bool b : Used ) if( !b ) { AnyUnused = true; break; }

		if( AnyUnused )
		{
			int idx = 0;

			vector<int> Change( mesh.mat.size(), -1 );
			for( int i = 0; i < mesh.mat.size(); i++ )
				if( Used[i] )
				{
					mesh.mat[idx] = mesh.mat[i];
					Change[i] = idx++;
				}

			mesh.mat.resize( idx );

			for( auto &a : mesh.faces ) a.material = Change[a.material];
		}
	}

	{
		vector<bool> Used( mesh.textures.size() );
		for( auto &a : mesh.mat ) Used[a.tTex] = true;

		bool AnyUnused = false;
		for( bool b : Used ) if( !b ) { AnyUnused = true; break; }

		if( AnyUnused )
		{
			int idx = 0;

			vector<int> Change( mesh.textures.size(), -1 );
			for( int i = 0; i < mesh.textures.size(); i++ )
				if( Used[i] )
				{
					mesh.textures[idx] = mesh.textures[i];
					Change[i] = idx++;
				}

			mesh.textures.resize( idx );

			for( auto &a : mesh.mat ) a.tTex = Change[a.tTex];
		}
	}

	return mesh;
}
*/