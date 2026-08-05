#include "background_content_builder.h"
#include "app_container.h"
#include "data/db/event_dao.h"
#include "data/db/task_dao.h"
#include "data/db/issue_dao.h"
#include "data/prefs/prefs.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <ranges>
#include <regex>

namespace cd {
namespace {
EpochMillis now_ms() { using namespace std::chrono; return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count(); }
bool ci_equal(std::string a, std::string b) { for(char& c:a)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));for(char& c:b)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return a==b; }
bool has_tag(std::vector<std::string> const& values, std::string const& tag) { return std::ranges::any_of(values,[&](auto const& v){return ci_equal(v,tag);}); }
bool date_matches(std::optional<EpochMillis> value, std::string const& filter) {
    if (filter == "all") return true;
    if (!value) return false;
    GDateTime* now=g_date_time_new_now_local(); GDateTime* today=g_date_time_new_local(g_date_time_get_year(now),g_date_time_get_month(now),g_date_time_get_day_of_month(now),0,0,0);
    GDateTime* tomorrow=g_date_time_add_days(today,1); GDateTime* after=g_date_time_add_days(today,2); GDateTime* week=g_date_time_add_days(today,-(g_date_time_get_day_of_week(today)-1)); GDateTime* next=g_date_time_add_days(week,7);
    auto ms=[](GDateTime* d){return EpochMillis(g_date_time_to_unix(d))*1000;};
    bool ok=filter=="today"?*value>=ms(today)&&*value<ms(tomorrow):filter=="tomorrow"?*value>=ms(tomorrow)&&*value<ms(after):*value>=ms(week)&&*value<ms(next);
    g_date_time_unref(next);g_date_time_unref(week);g_date_time_unref(after);g_date_time_unref(tomorrow);g_date_time_unref(today);g_date_time_unref(now);return ok;
}
std::string format_time(EpochMillis value) { GDateTime* d=g_date_time_new_from_unix_local(value/1000); gchar* s=g_date_time_format(d,"%b %e · %R"); std::string out=s?s:"";g_free(s);g_date_time_unref(d);return out; }
int estimate_minutes(std::string const& value) {
    int total{};
    try {
        std::regex const hours(R"((^|\s)#(\d+)\s*h\b)",std::regex::icase), minutes(R"((^|\s)#(\d+)\s*m\b)",std::regex::icase);
        for(std::sregex_iterator i(value.begin(),value.end(),hours),e;i!=e;++i)total+=std::stoi((*i)[2].str())*60;
        for(std::sregex_iterator i(value.begin(),value.end(),minutes),e;i!=e;++i)total+=std::stoi((*i)[2].str());
    } catch(...) {}
    return total;
}
}

BackgroundContent build_background_content(AppContainer& app, BackgroundTemplate const& t) {
    BackgroundContent out; out.title=t.source==BackgroundSource::Inbox?"INBOX":"VIEWS";
    out.filters=t.source==BackgroundSource::Inbox?t.inbox_type+" · "+t.inbox_date:t.views_type+" · "+t.views_date; out.mode=t.views_mode;
    auto const now=now_ms();
    if (t.source==BackgroundSource::Inbox) {
        auto const horizon=now+7LL*24*60*60*1000;
        if(t.inbox_type=="all"||t.inbox_type=="events") for(auto const& e:EventDao(app.db()).get_all()) if(e.start<=horizon&&std::max(e.end,e.start)>now&&date_matches(e.start,t.inbox_date)){out.rows.push_back({e.summary,format_time(e.start),"",0,false});out.total_minutes+=estimate_minutes(e.summary+" "+e.description.value_or(""));}
        if(t.inbox_type=="all"||t.inbox_type=="tasks") for(auto const& v:TaskDao(app.db()).get_all()) if(v.status!=TaskStatus::Completed&&date_matches(v.due,t.inbox_date)){out.rows.push_back({v.summary,v.due?format_time(*v.due):"No due date","",1,v.due&&*v.due<now});std::string tags=v.summary+" "+v.description.value_or("");for(auto const& tag:v.categories)tags+=" #"+tag;out.total_minutes+=estimate_minutes(tags);}
        if(t.inbox_type=="all"||t.inbox_type=="issues") for(auto const& v:IssueDao(app.db()).get_all()) if(v.state=="open"&&date_matches(v.milestone_due_on,t.inbox_date)){out.rows.push_back({v.title,v.repository,"",2,false});std::string tags=v.title+" "+v.body;for(auto const& tag:v.labels)tags+=" #"+tag;out.total_minutes+=estimate_minutes(tags);}
        std::stable_sort(out.rows.begin(),out.rows.end(),[](auto const& a,auto const& b){return a.title<b.title;});
    } else {
        auto settings=merged_app_preferences(app.prefs()); auto const& groups=settings.kanban_columns;
        if(t.views_type!="issues") for(auto const& v:TaskDao(app.db()).get_all()) if(v.status!=TaskStatus::Completed&&v.status!=TaskStatus::Cancelled&&date_matches(v.due,t.views_date)){
            std::string group;
            if(t.views_mode=="covey") { if(has_tag(v.categories,"do"))group="Do First";else if(has_tag(v.categories,"delay"))group="Schedule";else if(has_tag(v.categories,"delegate"))group="Delegate";else if(has_tag(v.categories,"eliminate"))group="Eliminate";else group="Untagged"; }
            else { auto i=std::ranges::find_if(groups,[&](auto const& g){return has_tag(v.categories,g);}); group=i==groups.end()?"Untagged":*i; }
            out.rows.push_back({v.summary,v.due?format_time(*v.due):"Task",group,1,v.due&&*v.due<now});
        }
        if(t.views_type!="tasks") for(auto const& v:IssueDao(app.db()).get_all()) if(v.state=="open"&&date_matches(v.milestone_due_on,t.views_date)){
            std::string group="Untagged";
            if(t.views_mode=="covey") { if(has_tag(v.labels,"do"))group="Do First";else if(has_tag(v.labels,"delay"))group="Schedule";else if(has_tag(v.labels,"delegate"))group="Delegate";else if(has_tag(v.labels,"eliminate"))group="Eliminate"; }
            else { auto i=std::ranges::find_if(groups,[&](auto const& g){return has_tag(v.labels,g);});if(i!=groups.end())group=*i; }
            if(t.views_mode!="covey"||group!="Untagged")out.rows.push_back({v.title,v.repository,group,2,false});
        }
    }
    return out;
}
}
