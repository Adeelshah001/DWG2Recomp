// Original procedural Windows keyboard artwork. No game pixels or fonts bundled.
#pragma once
#include <windows.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>
namespace dwg2::keyboard {
using Bindings=std::array<unsigned,25>;
inline std::wstring KeyName(unsigned packed,bool physical=false) {
 const unsigned key=packed&255;std::wstring result;
 if(!key)return L"--";
 if(!physical){if(packed&0x100)result+=L"S+";if(packed&0x200)result+=L"C+";if(packed&0x400)result+=L"A+";}
 if((key>='A'&&key<='Z')||(key>='0'&&key<='9'))return result+wchar_t(key);
 if(key>=VK_NUMPAD0&&key<=VK_NUMPAD9)return result+(physical?L"":L"N")+wchar_t(L'0'+key-VK_NUMPAD0);
 if(key>=VK_F1&&key<=VK_F12)return result+L"F"+std::to_wstring(key-VK_F1+1);
 switch(key){
 case VK_SPACE:return result+L"SPC";case VK_BACK:return result+L"BKSP";case VK_RETURN:return result+L"ENT";case VK_ESCAPE:return result+L"ESC";
 case VK_TAB:return result+L"TAB";case VK_CAPITAL:return result+L"CAPS";case VK_SHIFT:case VK_LSHIFT:case VK_RSHIFT:return result+L"SHIFT";
 case VK_CONTROL:case VK_LCONTROL:case VK_RCONTROL:return result+L"CTRL";case VK_MENU:case VK_LMENU:case VK_RMENU:return result+L"ALT";
 case VK_LEFT:return result+L"\u2190";case VK_RIGHT:return result+L"\u2192";case VK_UP:return result+L"\u2191";case VK_DOWN:return result+L"\u2193";
 case VK_INSERT:return result+L"INS";case VK_DELETE:return result+L"DEL";case VK_HOME:return result+L"HOME";case VK_END:return result+L"END";
 case VK_PRIOR:return result+L"PGUP";case VK_NEXT:return result+L"PGDN";case VK_NUMLOCK:return result+L"NUM";
 case VK_ADD:return result+L"+";case VK_SUBTRACT:return result+L"-";case VK_MULTIPLY:return result+L"*";case VK_DIVIDE:return result+L"/";case VK_DECIMAL:return result+L".";
 case VK_LWIN:case VK_RWIN:return result+L"WIN";case VK_APPS:return result+L"MENU";case VK_SNAPSHOT:return result+L"PRT";case VK_SCROLL:return result+L"SCR";case VK_PAUSE:return result+L"PAU";
 case 0xBA:return result+L";";case 0xBB:return result+L"=";case 0xBC:return result+L",";case 0xBD:return result+L"-";case 0xBE:return result+L".";case 0xBF:return result+L"/";
 case 0xC0:return result+L"`";case 0xDB:return result+L"[";case 0xDC:return result+L"\\";case 0xDD:return result+L"]";case 0xDE:return result+L"'";
 default:return result+L"K"+std::to_wstring(key);
 }
}
class Canvas {
 HDC dc_=nullptr;HBITMAP bitmap_=nullptr;HGDIOBJ previous_=nullptr;uint8_t* bits_=nullptr;
 public:int width,height;
 Canvas(int w,int h):width(w),height(h){BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;dc_=CreateCompatibleDC(nullptr);bitmap_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,(void**)&bits_,nullptr,0);if(dc_&&bitmap_){previous_=SelectObject(dc_,bitmap_);std::fill(bits_,bits_+size_t(w)*h*4,0);}}
 ~Canvas(){if(dc_&&previous_)SelectObject(dc_,previous_);if(bitmap_)DeleteObject(bitmap_);if(dc_)DeleteDC(dc_);}
 bool valid()const{return dc_&&bitmap_&&bits_;}
 void Key(int x,int y,int w,int h,const std::wstring& text,bool active){if(!valid())return;auto pen=CreatePen(PS_SOLID,1,active?RGB(233,204,151):RGB(164,164,164));auto brush=CreateSolidBrush(active?RGB(109,84,45):RGB(36,36,36));auto old_pen=SelectObject(dc_,pen),old_brush=SelectObject(dc_,brush);RoundRect(dc_,x,y,x+w,y+h,3,3);SelectObject(dc_,old_pen);SelectObject(dc_,old_brush);DeleteObject(pen);DeleteObject(brush);Label(x+1,y+1,w-2,h-2,text,std::max(5,std::min(h*3/5,int((w-2)*1.5/std::max<size_t>(1,text.size())))));}
 void Label(int x,int y,int w,int h,const std::wstring& text,int size){if(!valid())return;auto font=CreateFontW(-size,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto old=SelectObject(dc_,font);SetBkMode(dc_,TRANSPARENT);SetTextColor(dc_,RGB(245,238,220));RECT r{x,y,x+w,y+h};DrawTextW(dc_,text.c_str(),int(text.size()),&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);SelectObject(dc_,old);DeleteObject(font);}
 void Line(int x,int y,int end_x,int end_y){auto pen=CreatePen(PS_SOLID,1,RGB(200,171,115));auto old=SelectObject(dc_,pen);MoveToEx(dc_,x,y,nullptr);LineTo(dc_,end_x,end_y);const double angle=std::atan2(double(end_y-y),double(end_x-x));for(double side:{-.5,.5}){MoveToEx(dc_,end_x,end_y,nullptr);LineTo(dc_,int(end_x-5*std::cos(angle+side)),int(end_y-5*std::sin(angle+side)));}SelectObject(dc_,old);DeleteObject(pen);}
 std::vector<uint8_t> Pixels(){if(!valid())return {};GdiFlush();std::vector<uint8_t> out(size_t(width)*height*4);for(size_t i=0;i<out.size();i+=4){out[i]=bits_[i+2];out[i+1]=bits_[i+1];out[i+2]=bits_[i];out[i+3]=(bits_[i]|bits_[i+1]|bits_[i+2])?255:0;}return out;}
};
inline std::vector<uint8_t> Keycap(int width,int height,unsigned binding,const std::wstring& label=L"") {Canvas image(width,height);image.Key(0,0,width,height,label.empty()?KeyName(binding):label,false);return image.Pixels();}
inline std::wstring BindingLabel(unsigned primary,unsigned alternate){auto text=KeyName(primary);if(alternate&&alternate!=primary){if(!primary)return KeyName(alternate);text+=L" / "+KeyName(alternate);}return text;}
inline bool Prompts(std::vector<uint8_t>& pixels,int width,int height,int kind,const Bindings& bindings,const Bindings& alternate={}) {
 if(pixels.size()!=size_t(width)*height*4)return false;
 auto stamp=[&](int x,int y,int w,int h,unsigned action){if(action>=bindings.size()||x<0||y<0||x+w>width||y+h>height)return false;auto image=Keycap(w,h,bindings[action],BindingLabel(bindings[action],alternate[action]));if(image.size()!=size_t(w)*h*4)return false;for(int row=0;row<h;++row)std::copy_n(image.data()+size_t(row)*w*4,size_t(w)*4,pixels.data()+(size_t(y+row)*width+x)*4);return true;};
 if(kind==2&&width==512&&height==256)return stamp(72,72,72,72,0)&&stamp(0,72,72,72,1)&&stamp(396,0,72,72,2)&&stamp(324,0,72,72,3);
 if(kind==3&&width==1024&&height==512)return stamp(298,250,24,24,0)&&stamp(326,250,24,24,1)&&stamp(707,166,24,24,2)&&stamp(395,178,24,24,3)&&stamp(422,208,25,20,4)&&stamp(918,98,26,23,5)&&stamp(397,206,20,24,6)&&stamp(846,143,21,25,7)&&stamp(298,222,24,24,11)&&stamp(675,201,24,24,10)&&stamp(439,175,36,17,9)&&stamp(791,139,32,17,8);
 if(kind==4&&width==1024&&height==512)return stamp(144,207,64,29,5)&&stamp(687,170,46,28,4);
 return false;
}
inline std::vector<uint8_t> Diagram(const Bindings& bindings,const Bindings& alternate={}) {
 Canvas image(512,256);std::map<unsigned,std::pair<int,int>> centers;
 auto key=[&](unsigned vk,float unit,int y,float units=1,float rows=1){const int x=26+int(unit*20),w=int(units*20)-2,h=int(rows*26)-8;bool active=false;
  for(unsigned index=0;index<bindings.size()*2;++index){unsigned b=index<bindings.size()?bindings[index]:alternate[index-bindings.size()];active|=(b&255)==vk||(((b&0x100)||(b&255)==VK_SHIFT)&&(vk==VK_LSHIFT||vk==VK_RSHIFT))||(((b&0x200)||(b&255)==VK_CONTROL)&&(vk==VK_LCONTROL||vk==VK_RCONTROL))||(((b&0x400)||(b&255)==VK_MENU)&&(vk==VK_LMENU||vk==VK_RMENU));}
  image.Key(x,y,w,h,KeyName(vk,true),active);centers.try_emplace(vk,std::pair{x+w/2,y+h/2});};
 key(VK_ESCAPE,0,12);for(unsigned i=0;i<12;++i)key(VK_F1+i,2+i+(i>=4?.5f:0)+(i>=8?.5f:0),12);
 for(unsigned i=0;i<3;++i)key(std::array<unsigned,3>{VK_SNAPSHOT,VK_SCROLL,VK_PAUSE}[i],15.5f+i,12);
 const unsigned number[]={0xC0,'1','2','3','4','5','6','7','8','9','0',0xBD,0xBB};for(unsigned i=0;i<13;++i)key(number[i],float(i),54);key(VK_BACK,13,54,2);
 key(VK_TAB,0,80,1.5);const char* q="QWERTYUIOP";for(unsigned i=0;i<10;++i)key(q[i],1.5f+i,80);key(0xDB,11.5,80);key(0xDD,12.5,80);key(0xDC,13.5,80,1.5);
 key(VK_CAPITAL,0,106,1.75);const char* a="ASDFGHJKL";for(unsigned i=0;i<9;++i)key(a[i],1.75f+i,106);key(0xBA,10.75,106);key(0xDE,11.75,106);key(VK_RETURN,12.75,106,2.25);
 key(VK_LSHIFT,0,132,2.25);const char* z="ZXCVBNM";for(unsigned i=0;i<7;++i)key(z[i],2.25f+i,132);key(0xBC,9.25,132);key(0xBE,10.25,132);key(0xBF,11.25,132);key(VK_RSHIFT,12.25,132,2.75);
 const unsigned bottom[]={VK_LCONTROL,VK_LWIN,VK_LMENU,VK_SPACE,VK_RMENU,VK_RWIN,VK_APPS,VK_RCONTROL};float x=0;for(unsigned i=0;i<8;++i){float units=i==3?6.25f:1.25f;key(bottom[i],x,158,units);x+=units;}
 for(unsigned i=0;i<3;++i){key(std::array<unsigned,3>{VK_INSERT,VK_HOME,VK_PRIOR}[i],15.5f+i,54);key(std::array<unsigned,3>{VK_DELETE,VK_END,VK_NEXT}[i],15.5f+i,80);key(std::array<unsigned,3>{VK_LEFT,VK_DOWN,VK_RIGHT}[i],15.5f+i,158);}key(VK_UP,16.5,132);
 for(unsigned i=0;i<4;++i)key(std::array<unsigned,4>{VK_NUMLOCK,VK_DIVIDE,VK_MULTIPLY,VK_SUBTRACT}[i],19.0f+i,54);
 for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)key(VK_NUMPAD7-row*3+col,19.0f+col,80+26*row);
 key(VK_ADD,22,80,1,2);key(VK_RETURN,22,132,1,2);key(VK_NUMPAD0,19,158,2);key(VK_DECIMAL,21,158);
 centers[VK_SHIFT]=centers[VK_LSHIFT];centers[VK_CONTROL]=centers[VK_LCONTROL];centers[VK_MENU]=centers[VK_LMENU];
 for(unsigned i=0;i<4;++i){const unsigned binding=bindings[i];auto found=centers.find(binding&255);if(found==centers.end())continue;const int label_x=26+int(i)*118;image.Label(label_x,218,100,20,BindingLabel(binding,alternate[i]),12);image.Line(label_x+50,216,found->second.first,found->second.second);if(alternate[i]&&alternate[i]!=binding){auto other=centers.find(alternate[i]&255);if(other!=centers.end())image.Line(label_x+50,216,other->second.first,other->second.second);}}
 image.Label(26,191,460,18,L"Mapped keys",12);return image.Pixels();
}
// Encode our own RGBA diagram to the controller texture's existing BC3 format.
inline std::vector<uint8_t> Bc3(const std::vector<uint8_t>& rgba,int width,int height) {
 if(width%4||height%4||rgba.size()!=size_t(width)*height*4)return {};std::vector<uint8_t> out(size_t(width)*height);
 for(int y=0;y<height;y+=4)for(int x=0;x<width;x+=4){auto* block=out.data()+(size_t(y/4)*(width/4)+x/4)*16;block[0]=255;block[1]=0;uint64_t alpha_bits=0;uint32_t color_bits=0;unsigned low[3]={255,255,255},high[3]={0,0,0};
  for(unsigned i=0;i<16;++i){const auto* p=rgba.data()+(size_t(y+i/4)*width+x+i%4)*4;for(unsigned c=0;c<3;++c){low[c]=std::min(low[c],unsigned(p[c]));high[c]=std::max(high[c],unsigned(p[c]));}unsigned a=unsigned(std::round((255-p[3])*7./255));unsigned index=a==0?0:a==7?1:a+1;alpha_bits|=uint64_t(index)<<(i*3);}
  for(unsigned i=0;i<6;++i)block[2+i]=uint8_t(alpha_bits>>(i*8));
  auto rgb565=[](const unsigned* c){return uint16_t(((c[0]>>3)<<11)|((c[1]>>2)<<5)|(c[2]>>3));};uint16_t c0=rgb565(high),c1=rgb565(low);if(c0<c1)std::swap(c0,c1);block[8]=uint8_t(c0);block[9]=uint8_t(c0>>8);block[10]=uint8_t(c1);block[11]=uint8_t(c1>>8);
  unsigned colors[4][3]={{unsigned((c0>>11)&31)*255/31,unsigned((c0>>5)&63)*255/63,unsigned(c0&31)*255/31},{unsigned((c1>>11)&31)*255/31,unsigned((c1>>5)&63)*255/63,unsigned(c1&31)*255/31}};
  for(unsigned c=0;c<3;++c){colors[2][c]=(2*colors[0][c]+colors[1][c])/3;colors[3][c]=(colors[0][c]+2*colors[1][c])/3;}
  for(unsigned i=0;i<16;++i){const auto* p=rgba.data()+(size_t(y+i/4)*width+x+i%4)*4;unsigned best=0;int error_best=1000000;for(unsigned k=0;k<4;++k){int error=0;for(unsigned c=0;c<3;++c){int d=int(p[c])-int(colors[k][c]);error+=d*d;}if(error<error_best){best=k;error_best=error;}}color_bits|=best<<(i*2);}for(unsigned i=0;i<4;++i)block[12+i]=uint8_t(color_bits>>(i*8));
 }return out;
}
}
