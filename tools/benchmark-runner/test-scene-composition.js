#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {execFileSync} = require('node:child_process');

const root = path.resolve(__dirname, '../..');
const cpp = fs.readFileSync(path.join(root, 'src/benchmark/MotionMarkBenches.cpp'), 'utf8');
const header = fs.readFileSync(path.join(root, 'src/benchmark/MotionMarkBenches.h'), 'utf8');
const declarations = header.slice(header.indexOf('namespace benchmark {'))
    .replaceAll(' private:', ' public:').replaceAll(' protected:', ' public:');
const helpers = cpp.slice(cpp.indexOf('constexpr std::array<float, kShaderMixMotifCount> ShaderMixScales'),
  cpp.indexOf('tgfx::Color Hsl('));
let methods = cpp.slice(cpp.indexOf('MultiplyBench::MultiplyBench()'), cpp.lastIndexOf('}  // namespace benchmark'));
if (process.argv.includes('--negative-control')) {
  const start = methods.indexOf('void ImagesBench::onDraw');
  const end = methods.indexOf('DesignBench::DesignBench', start);
  methods = methods.slice(0, start) + methods.slice(start, end)
      .replace('i < count', 'i < 1') + methods.slice(end);
}

// Record the real scene loops and transforms without creating a GPU device.
const harness = `
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace tgfx {
struct Color { float red=0, green=0, blue=0, alpha=1;
  static Color Black(){return {};} static Color White(){return {1,1,1,1};}
  static Color Red(){return {1,0,0,1};} };
struct Point { float x=0, y=0; };
struct Rect {
  float left=0, top=0, right=0, bottom=0;
  static Rect MakeWH(float w,float h){return {0,0,w,h};}
  static Rect MakeXYWH(float x,float y,float w,float h){return {x,y,x+w,y+h};}
  static Rect MakeEmpty(){return {};}
  float width() const{return right-left;} float height() const{return bottom-top;}
};
struct Image { int width() const{return 100;} int height() const{return 100;} };
struct Font {
  std::shared_ptr<int> typeface; float size=0;
  Font()=default; Font(std::shared_ptr<int> t,float s):typeface(t),size(s){}
  bool operator==(const Font& f) const{return typeface==f.typeface && size==f.size;}
};
struct TextBlob {
  Rect bounds;
  static std::shared_ptr<TextBlob> MakeFrom(const std::string& s,const Font& f){
    size_t glyphs=0; for(unsigned char c:s) if((c&0xc0)!=0x80) ++glyphs;
    return std::make_shared<TextBlob>(TextBlob{Rect::MakeWH(float(glyphs)*f.size*0.65f,f.size)});
  }
  Rect getBounds() const{return bounds;}
};
struct Shader { static int MakeLinearGradient(Point,Point,std::vector<Color>,std::vector<float> = {}){return 0;} };
struct Paint {
  void setAlpha(float){} void setShader(int){} void setLineCap(int){}
};
struct LineCap { static constexpr int Square=1; };
struct Path {
  Rect bounds;
  void addArc(Rect r,float,float){bounds=r;} void lineTo(float,float){} void close(){}
};
struct Canvas {
  struct Matrix { float a=1,b=0,c=0,d=1,x=0,y=0; };
  struct Draw { Rect bounds; std::string kind; };
  Matrix matrix; std::vector<Matrix> stack; std::vector<Draw> draws;
  std::array<size_t,13> motifs{};
  void save(){stack.push_back(matrix);} void restore(){matrix=stack.back();stack.pop_back();}
  void translate(float x,float y){matrix.x+=matrix.a*x+matrix.c*y;matrix.y+=matrix.b*x+matrix.d*y;}
  void rotate(float degrees){
    const auto angle=degrees*3.14159265f/180; const auto cs=std::cos(angle),sn=std::sin(angle);
    const auto old=matrix;
    matrix.a=old.a*cs+old.c*sn;matrix.b=old.b*cs+old.d*sn;
    matrix.c=old.c*cs-old.a*sn;matrix.d=old.d*cs-old.b*sn;
  }
  void record(Rect r,const std::string& kind){
    Rect transformed{1e9f,1e9f,-1e9f,-1e9f};
    for(float x:{r.left,r.right}) for(float y:{r.top,r.bottom}){
      const auto px=matrix.a*x+matrix.c*y+matrix.x,py=matrix.b*x+matrix.d*y+matrix.y;
      transformed.left=std::min(transformed.left,px);transformed.right=std::max(transformed.right,px);
      transformed.top=std::min(transformed.top,py);transformed.bottom=std::max(transformed.bottom,py);
    }
    draws.push_back({transformed,kind});
  }
  void drawRect(Rect r,Paint){record(r,"rect");}
  void drawRoundRect(Rect r,float,float,Paint){record(r,"roundRect");}
  void drawOval(Rect r,Paint){record(r,"oval");}
  void drawCircle(float x,float y,float r,Paint){record(Rect::MakeXYWH(x-r,y-r,r*2,r*2),"circle");}
  void drawPath(Path p,Paint){record(p.bounds,"path");}
  void drawLine(float x,float y,float ex,float ey,Paint){record({std::min(x,ex),std::min(y,ey),std::max(x,ex),std::max(y,ey)},"line");}
  void drawImageRect(std::shared_ptr<Image>,Rect,Rect r,int,const Paint*){record(r,"image");}
  void drawSimpleText(const std::string& s,float x,float y,Font f,Paint){
    const auto r=TextBlob::MakeFrom(s,f)->getBounds(); record(Rect::MakeXYWH(x,y-f.size,r.width(),f.size),"text");
  }
};
}
namespace benchmark {
struct AppHost {
  int w,h; float dpr=1;
  int width() const{return w;} int height() const{return h;} float density() const{return dpr;}
  std::shared_ptr<int> getTypeface(const char*) const{static auto font=std::make_shared<int>(1);return font;}
};
class Bench {
 public:
  explicit Bench(std::string){} virtual ~Bench()=default;
  uint64_t frame=1;double animationFrame=0;float animationDelta=1;
  virtual void setCount(size_t){} virtual size_t getCount() const{return 0;}
  virtual void onDraw(tgfx::Canvas*,const AppHost*)=0;
};
}
${declarations}
namespace benchmark {
constexpr float PI=3.14159265358979323846f;
constexpr size_t kShaderMixMotifCount=13;
constexpr std::array<int,13> ImageNames={};
${helpers}
tgfx::Color Hsl(float,float,float,float=1){return {};}
tgfx::Paint Fill(tgfx::Color){return {};}
tgfx::Paint Stroke(tgfx::Color,float){return {};}
std::shared_ptr<tgfx::Image> MotionMarkImage(const AppHost*,size_t){static auto image=std::make_shared<tgfx::Image>();return image;}
tgfx::Rect ImageSource(const std::shared_ptr<tgfx::Image>&){return tgfx::Rect::MakeWH(100,100);}
tgfx::Path SuitPath(size_t,tgfx::Rect bounds){return {bounds};}
void DrawShaderMixBackdrop(tgfx::Canvas* c,float w,float h,float){c->drawRect(tgfx::Rect::MakeWH(w,h),{});}
void DrawShaderMixMotif(tgfx::Canvas* c,const ShaderMixTile& t,size_t,tgfx::Rect r,float,tgfx::Font,const AppHost*,bool){
  ++c->motifs[t.motif];c->record(r,"motif");
}
${methods}
template<class Scene> void check(const char* name,AppHost host,size_t count,size_t expected){
  Scene scene;scene.setCount(count);tgfx::Canvas canvas;scene.onDraw(&canvas,&host);
  if(canvas.draws.size()!=expected) throw std::runtime_error(std::string(name)+": skipped objects");
  const auto first=canvas.draws;
  bool animated=false;
  for(int frame=1;frame<=360;++frame){
    scene.frame=uint64_t(frame+1);scene.animationFrame=frame;canvas.draws.clear();canvas.motifs={};
    scene.onDraw(&canvas,&host);
    if(canvas.draws.size()!=expected || !canvas.stack.empty()) throw std::runtime_error(std::string(name)+": draw count or save/restore changed");
    for(size_t i=1;i<canvas.draws.size();++i){
      const auto& r=canvas.draws[i].bounds;
      if(!std::isfinite(r.left+r.top+r.right+r.bottom) || r.right<0 || r.bottom<0 || r.left>host.w || r.top>host.h)
        throw std::runtime_error(std::string(name)+": fully offscreen object");
      if(r.left!=first[i].bounds.left || r.top!=first[i].bounds.top ||
          r.right!=first[i].bounds.right || r.bottom!=first[i].bounds.bottom) animated=true;
    }
    if constexpr(std::is_same_v<Scene,ShaderMixBench>){
      if(count>=13) for(auto uses:canvas.motifs) if(uses==0) throw std::runtime_error("Shader Mix: missing motif");
    }
  }
  if(!animated) throw std::runtime_error(std::string(name)+": static animation");
}
}
int main(){
  try {
    using namespace benchmark;
    for(const auto host:{AppHost{1000,600},AppHost{2560,1440,2},AppHost{320,480},
                         AppHost{390,332},AppHost{640,960,2},AppHost{780,664,2}}){
      for(size_t count:{size_t(8),size_t(64),size_t(256)}){
        check<MultiplyBench>("Multiply",host,count,count+1);
        check<CanvasArcsBench>("Arcs",host,count,count+1);
        check<LeavesBench>("Leaves",host,count,count+1);
        check<CanvasLinesBench>("Lines",host,count,count+13);
        check<ImagesBench>("Images",host,count,count+1);
        check<DesignBench>("Design",host,count,count*12+1);
        check<SuitsBench>("Suits",host,count,count+1);
        check<ShaderMixBench>("Shader Mix",host,count,count+2);
      }
    }
    std::cout<<"scene composition: eight scene loops, visible objects, animation, count units and 13 motif dispatches passed\\n";
  } catch(const std::exception& e){std::cerr<<e.what()<<"\\n";return 1;}
}
`;

const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'tgfx-scene-composition-'));
try {
  const source = path.join(temporary, 'test.cpp');
  const executable = path.join(temporary, 'test');
  fs.writeFileSync(source, harness);
  execFileSync(process.env.CXX || 'clang++', ['-std=c++17', '-O1', source, '-o', executable],
    {stdio: 'inherit'});
  if (process.argv.includes('--negative-control')) {
    assert.throws(() => execFileSync(executable, {stdio: 'pipe'}), error =>
      error.stderr.toString().includes('Images: skipped objects'));
    console.log('negative control: adding counts without drawing them correctly fails');
  } else {
    execFileSync(executable, {stdio: 'inherit'});
  }
} finally {
  fs.rmSync(temporary, {recursive: true, force: true});
}
