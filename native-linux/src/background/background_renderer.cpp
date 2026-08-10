#include "background_renderer.h"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <filesystem>
#include <span>
#include <ctime>
#include <algorithm>

namespace cd {
namespace {
void color(cairo_t* c, unsigned rgb){cairo_set_source_rgb(c,((rgb>>16)&255)/255.0,((rgb>>8)&255)/255.0,(rgb&255)/255.0);}
void text(cairo_t* c,char const* font,double size,unsigned rgb,double x,double y,std::string const& value,int width){auto* l=pango_cairo_create_layout(c);auto* d=pango_font_description_from_string(font);pango_font_description_set_absolute_size(d,size*PANGO_SCALE);pango_layout_set_font_description(l,d);pango_layout_set_text(l,value.c_str(),-1);pango_layout_set_width(l,width*PANGO_SCALE);pango_layout_set_ellipsize(l,PANGO_ELLIPSIZE_END);color(c,rgb);cairo_move_to(c,x,y);pango_cairo_show_layout(c,l);pango_font_description_free(d);g_object_unref(l);}
void magic_tag(cairo_t* c,double x,double y,double width,std::string const& value){color(c,0x3B342A);cairo_rectangle(c,x,y,width,46);cairo_fill(c);text(c,"Sans Bold",24,0xF2B35D,x+14,y+8,"#"+value,width-28);}
void board_panel(cairo_t* c,BackgroundContent const& content,std::string const& group,double x,double y,double width,double height){
    color(c,0x18243A);cairo_rectangle(c,x,y,width,height);cairo_fill(c);magic_tag(c,x+20,y+18,width-90,group);
    std::vector<BackgroundRow const*> rows;for(auto const& row:content.rows)if(row.group==group)rows.push_back(&row);text(c,"Sans",24,0x9AABC2,x+width-52,y+27,std::to_string(rows.size()),35);
    int available=std::max(1,int((height-105)/92));double row_y=y+82;for(int i=0;i<std::min<int>(available,rows.size());++i){auto const& row=*rows[i];color(c,row.overdue?0x4B2E39:0x22324D);cairo_rectangle(c,x+16,row_y,width-32,76);cairo_fill(c);text(c,"Sans Semi-Bold",27,0xEAF0F7,x+30,row_y+10,row.title,width-60);text(c,"Sans",19,0x9AABC2,x+30,row_y+45,row.subtitle,width-60);row_y+=88;}if(int(rows.size())>available)text(c,"Sans",20,0x9AABC2,x+20,y+height-38,"+"+std::to_string(rows.size()-available)+" more",width-40);
}
}
bool render_background_png(BackgroundContent const& v,std::string const& path,std::string& error){
    constexpr int W=3840,H=2160; std::filesystem::create_directories(std::filesystem::path(path).parent_path()); std::string tmp=path+".tmp";
    cairo_surface_t* s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,W,H);cairo_t* c=cairo_create(s);color(c,0x0B1220);cairo_paint(c);
    auto now=std::time(nullptr);char stamp[16]{};std::strftime(stamp,sizeof(stamp),"%H:%M",std::localtime(&now));
    text(c,"Sans Bold",112,0xEAF0F7,230,170,v.title,2600);text(c,"Sans",38,0x9AABC2,235,310,v.filters+(v.title=="VIEWS"?" · "+v.mode:"")+" · "+std::to_string(v.rows.size())+" visible",2800);text(c,"Sans",30,0x9AABC2,3200,185,std::string("UPDATED ")+stamp,430);
    if(v.title=="VIEWS"&&!v.groups.empty()){
        if(v.mode=="covey"){double gap=24,cell_w=(3380-gap)/2,cell_h=(1570-gap)/2;for(std::size_t i=0;i<std::min<std::size_t>(4,v.groups.size());++i)board_panel(c,v,v.groups[i],230+(i%2)*(cell_w+gap),430+(i/2)*(cell_h+gap),cell_w,cell_h);}
        else{std::size_t count=std::min<std::size_t>(7,v.groups.size());double gap=18,col_w=(3380-gap*(count-1))/count;for(std::size_t i=0;i<count;++i)board_panel(c,v,v.groups[i],230+i*(col_w+gap),430,col_w,1570);}
        cairo_status_t status=cairo_surface_write_to_png(s,tmp.c_str());cairo_destroy(c);cairo_surface_destroy(s);if(status!=CAIRO_STATUS_SUCCESS){error=cairo_status_to_string(status);return false;}std::error_code ec;std::filesystem::rename(tmp,path,ec);if(ec){error=ec.message();return false;}return true;
    }
    int y=430;auto rows=v.rows.size()>12?std::span(v.rows.data(),12):std::span(v.rows.data(),v.rows.size());
    if(rows.empty())text(c,"Sans",48,0xEAF0F7,250,520,"Nothing matches this snapshot",2800);
    for(auto const& r:rows){color(c,0x18243A);cairo_rectangle(c,230,y,3380,115);cairo_fill(c);unsigned accent=r.overdue?0xF27C7C:(r.kind==0?0x65C7D0:0xF2B35D);color(c,accent);cairo_rectangle(c,230,y,9,115);cairo_fill(c);text(c,"Sans Semi-Bold",42,0xEAF0F7,275,y+18,r.title,2450);text(c,"Sans",27,0x9AABC2,275,y+71,(r.group.empty()?"":r.group+" · ")+r.subtitle,2450);y+=132;}
    std::string footer;if(v.rows.size()>rows.size())footer="+"+std::to_string(v.rows.size()-rows.size())+" more";if(v.total_minutes>0){if(!footer.empty())footer+=" · ";footer+=std::to_string(v.total_minutes/60)+"h "+std::to_string(v.total_minutes%60)+"m estimated";}if(!footer.empty())text(c,"Sans",32,0x9AABC2,235,H-150,footer,1600);
    cairo_status_t status=cairo_surface_write_to_png(s,tmp.c_str());cairo_destroy(c);cairo_surface_destroy(s);if(status!=CAIRO_STATUS_SUCCESS){error=cairo_status_to_string(status);return false;}std::error_code ec;std::filesystem::rename(tmp,path,ec);if(ec){error=ec.message();return false;}return true;
}
}
