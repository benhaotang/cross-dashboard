#include "background_renderer.h"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <filesystem>
#include <span>
#include <ctime>

namespace cd {
namespace {
void color(cairo_t* c, unsigned rgb){cairo_set_source_rgb(c,((rgb>>16)&255)/255.0,((rgb>>8)&255)/255.0,(rgb&255)/255.0);}
void text(cairo_t* c,char const* font,double size,unsigned rgb,double x,double y,std::string const& value,int width){auto* l=pango_cairo_create_layout(c);auto* d=pango_font_description_from_string(font);pango_font_description_set_absolute_size(d,size*PANGO_SCALE);pango_layout_set_font_description(l,d);pango_layout_set_text(l,value.c_str(),-1);pango_layout_set_width(l,width*PANGO_SCALE);pango_layout_set_ellipsize(l,PANGO_ELLIPSIZE_END);color(c,rgb);cairo_move_to(c,x,y);pango_cairo_show_layout(c,l);pango_font_description_free(d);g_object_unref(l);}
}
bool render_background_png(BackgroundContent const& v,std::string const& path,std::string& error){
    constexpr int W=3840,H=2160; std::filesystem::create_directories(std::filesystem::path(path).parent_path()); std::string tmp=path+".tmp";
    cairo_surface_t* s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,W,H);cairo_t* c=cairo_create(s);color(c,0x0B1220);cairo_paint(c);
    auto now=std::time(nullptr);char stamp[16]{};std::strftime(stamp,sizeof(stamp),"%H:%M",std::localtime(&now));
    text(c,"Sans Bold",112,0xEAF0F7,230,170,v.title,2600);text(c,"Sans",38,0x9AABC2,235,310,v.filters+(v.title=="VIEWS"?" · "+v.mode:"")+" · "+std::to_string(v.rows.size())+" visible",2800);text(c,"Sans",30,0x9AABC2,3200,185,std::string("UPDATED ")+stamp,430);
    int y=430;auto rows=v.rows.size()>12?std::span(v.rows.data(),12):std::span(v.rows.data(),v.rows.size());
    if(rows.empty())text(c,"Sans",48,0xEAF0F7,250,520,"Nothing matches this snapshot",2800);
    for(auto const& r:rows){color(c,0x18243A);cairo_rectangle(c,230,y,3380,115);cairo_fill(c);unsigned accent=r.overdue?0xF27C7C:(r.kind==0?0x65C7D0:0xF2B35D);color(c,accent);cairo_rectangle(c,230,y,9,115);cairo_fill(c);text(c,"Sans Semi-Bold",42,0xEAF0F7,275,y+18,r.title,2450);text(c,"Sans",27,0x9AABC2,275,y+71,(r.group.empty()?"":r.group+" · ")+r.subtitle,2450);y+=132;}
    std::string footer;if(v.rows.size()>rows.size())footer="+"+std::to_string(v.rows.size()-rows.size())+" more";if(v.total_minutes>0){if(!footer.empty())footer+=" · ";footer+=std::to_string(v.total_minutes/60)+"h "+std::to_string(v.total_minutes%60)+"m estimated";}if(!footer.empty())text(c,"Sans",32,0x9AABC2,235,H-150,footer,1600);
    cairo_status_t status=cairo_surface_write_to_png(s,tmp.c_str());cairo_destroy(c);cairo_surface_destroy(s);if(status!=CAIRO_STATUS_SUCCESS){error=cairo_status_to_string(status);return false;}std::error_code ec;std::filesystem::rename(tmp,path,ec);if(ec){error=ec.message();return false;}return true;
}
}
