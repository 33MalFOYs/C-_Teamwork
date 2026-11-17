#include "expense_tracker/file_manager.hpp"
#include "expense_tracker/validation.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace expense_tracker {
namespace {
const std::string header="expense_id,amount,category,expense_date,description";
std::string quote(const std::string& value) {
    std::string result="\"";
    for(char c:value) { if(c=='\"') result+='\"'; result+=c; }
    return result+'\"';
}
std::vector<std::string> parse(const std::string& line) {
    std::vector<std::string> fields; std::string field;
    bool quoted=false, closed=false, started=false;
    for(char c:line) {
        if(quoted) {
            if(c=='\"') { quoted=false; closed=true; } else field+=c;
        } else if(closed && c=='\"') { field+='\"'; quoted=true; closed=false; }
        else if(c==',') { fields.push_back(field);field.clear();closed=false;started=false; }
        else if(closed) throw std::runtime_error("Unexpected character after closing quote.");
        else if(c=='\"') { if(started) throw std::runtime_error("Quote inside unquoted field."); quoted=true;started=true; }
        else { field+=c;started=true; }
    }
    if(quoted) throw std::runtime_error("Unclosed quote; embedded line breaks are not supported.");
    fields.push_back(field);return fields;
}
void write(const std::filesystem::path& path,const std::string& data,bool overwrite) {
    if(std::filesystem::exists(path) && !overwrite) throw std::runtime_error("Export exists. Confirm overwrite first.");
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    if(!stream) throw std::runtime_error("Cannot open export destination.");
    stream<<data;stream.flush();
    if(!stream) throw std::runtime_error("Export write failed.");
    stream.close();if(!stream) throw std::runtime_error("Export close failed.");
}
}
FileManager::FileManager(ExpenseService& expenses):expenses_(expenses) {}
FileManager::FileManager(const Session&,ExpenseService& expenses,ReportService&):expenses_(expenses) {}
void FileManager::importCsv(const std::filesystem::path& path) {
    (void)expenses_.matching(ExpenseFilter{});
    std::ifstream stream(path,std::ios::binary);
    if(!stream) throw std::runtime_error("Cannot open CSV input.");
    std::string line; std::size_t number=0;std::vector<ExpenseDraft> rows;
    while(std::getline(stream,line)) {
        ++number;if(!line.empty()&&line.back()=='\r') line.pop_back();
        if(number==1) {
            if(line.compare(0,3,"\xef\xbb\xbf")==0)line.erase(0,3);
            if(line!=header)throw std::runtime_error("CSV header must be: "+header);
            continue;
        }
        try {
            auto fields=parse(line);
            if(fields.size()!=5)throw std::runtime_error("Expected five columns.");
            if(!fields[0].empty()) (void)validation::parsePositiveId(fields[0]);
            ExpenseDraft draft{validation::parseAmount(fields[1]),fields[2],fields[3],fields[4]};
            Expense::validate(draft);rows.push_back(std::move(draft));
        } catch(const std::exception& e) { throw std::runtime_error("CSV row "+std::to_string(number)+": "+e.what()); }
    }
    if(stream.bad())throw std::runtime_error("Could not finish reading CSV.");
    if(number==0)throw std::runtime_error("CSV is empty; a header is required.");
    expenses_.importRows(rows);
}
void FileManager::exportCsv(const std::filesystem::path& path,const ExpenseFilter& filter,bool overwrite) const {
    auto rows=expenses_.matching(filter);std::ostringstream out;out<<header<<'\n';
    for(const auto& e:rows)out<<e.id()<<','<<validation::formatMoney(e.amount())<<','<<quote(e.category())<<','<<e.date()<<','<<quote(e.description())<<'\n';
    write(path,out.str(),overwrite);
}
void FileManager::exportWeeklyReport(const std::filesystem::path& path,const std::string& date,bool overwrite) const {
    auto report=expenses_.weekly(date);std::ostringstream out;
    out<<"section,label,value\nweek,start,"<<report.dates.start<<"\nweek,end,"<<report.dates.end
       <<"\nsummary,total_usd,"<<validation::formatMoney(report.total)<<"\nsummary,count,"<<report.count
       <<"\nsummary,daily_average_usd,"<<validation::formatDailyAverage(report.total)<<'\n';
    for(const auto& day:report.days)out<<"day,"<<day.date<<','<<validation::formatMoney(day.total)<<'\n';
    for(const auto& category:report.categories)out<<"category,"<<quote(category.first)<<','<<validation::formatMoney(category.second)<<'\n';
    write(path,out.str(),overwrite);
}
}
