#include "expense_tracker/file_manager.hpp"
#include "expense_tracker/auth_service.hpp"
#include "expense_tracker/config.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
using namespace expense_tracker;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Expected rejection");}
std::string read(const std::filesystem::path& p){std::ifstream f(p);std::ostringstream out;out<<f.rdbuf();return out.str();}
int main(int argc,char** argv){
 if(argc<2||!argv[1][0])return 77;
 try{
 auto config=loadDatabaseConfig(argv[1]);check(config.database.rfind("expense_tracker_test_",0)==0,"Test schema required");
 DatabaseManager db;db.connect(config);db.verifySchema();Session session;ExpenseList list;AuthService auth(db,session,list);ExpenseService expenses(session,db,list);FileManager files(expenses);
 auto suffix=std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());auto user="csv_"+suffix;
 const std::string password="Fictional CSV password!";auth.registerAccount(user,password,password);check(auth.signIn(user,password),"Login");
 const auto folder=std::filesystem::path("csv-test-"+suffix);std::filesystem::create_directory(folder);auto csv=folder/"expenses.csv", weekly=folder/"weekly.csv", invalid=folder/"invalid.csv";
 expenses.add({1000,"Food, shared","2026-12-28","O'Reilly, \"lunch\""});expenses.add({2000,"Travel","2026-12-30","ride"});expenses.add({500,"Food","2027-01-03","snack"});expenses.add({9900,"Other","2027-01-04","excluded"});
 files.exportCsv(csv,ExpenseFilter{});auto original=read(csv);check(original.find("\"O'Reilly, \"\"lunch\"\"\"")!=std::string::npos,"CSV quoting");
 rejects([&]{files.exportCsv(csv,ExpenseFilter{});});check(read(csv)==original,"Overwrite rejection changed file");
 files.exportWeeklyReport(weekly,"2026-12-31");auto summary=read(weekly);check(summary.find("summary,total_usd,35.00")!=std::string::npos,"Weekly total");check(summary.find("summary,count,3")!=std::string::npos,"Weekly count");check(summary.find("summary,daily_average_usd,5.00")!=std::string::npos,"Weekly average");check(summary.find("day,2027-01-04")==std::string::npos,"Next Monday excluded");
 const auto oldId=expenses.matching(ExpenseFilter{}).back().id();files.importCsv(csv);auto rows=expenses.matching(ExpenseFilter{});check(rows.size()==8&&rows[4].id()>oldId,"Imported IDs");check(rows[4].description()==rows[0].description(),"Roundtrip fields");
 auto bad=[&](const std::string& data){std::ofstream out(invalid);out<<data;out.close();rejects([&]{files.importCsv(invalid);});check(expenses.matching(ExpenseFilter{}).size()==8,"Partial invalid import");};
 const std::string header="expense_id,amount,category,expense_date,description\n";
 bad(header+"1,1.00,Food,2026-01-01,valid\n2,0.00,Food,2026-01-02,invalid\n");
 bad(header+"1,1.00,Food,2026-01-01,\"unclosed\n");bad(header+"1,1.00,Food,2026-01-01,\"closed\"junk\n");bad("bad header\n");bad("");bad(header+"1,1.00,Food,2026-01-01,too,many\n");
 rejects([&]{files.importCsv(folder/"missing.csv");});rejects([&]{files.exportCsv(folder/"missing"/"export.csv",ExpenseFilter{});});
 // Explicit overwrite, empty filtered export, BOM/CRLF and Unicode paths.
 files.exportCsv(csv,ExpenseFilter{},true);check(read(csv).find("expense_id,")==0,"Confirmed overwrite");
 auto unicodePath=folder/std::filesystem::u8path("\xe1\x9e\x80.csv");
 files.exportCsv(unicodePath,ExpenseFilter(DateRange{"2025-01-01","2025-01-01"},{}));
 check(read(unicodePath)=="expense_id,amount,category,expense_date,description\n","Empty filtered export");
 {std::ofstream bom(invalid,std::ios::binary);bom<<"\xef\xbb\xbf"<<"expense_id,amount,category,expense_date,description\r\n";}
 files.importCsv(invalid);check(expenses.matching(ExpenseFilter{}).size()==8,"Header-only import");
 bad(header+"1,1.00,Food,2026-02-30,invalid date\n");
 bad(header+"1,1.00,Food,2026-01-01,"+std::string(1,static_cast<char>(0xff))+"\n");
 expenses.setBudget({2026,12},1000);check(expenses.monthly({2026,12}).total==6000,"Overspending total");check(*expenses.budget({2026,12})-expenses.monthly({2026,12}).total==-5000,"Overspending remaining");
 if(std::getenv("EXPENSE_TEST_REJECT_CATEGORY")) {
 std::ofstream out(invalid);out<<header<<"1,1.00,Food,2026-01-01,rollback\n2,1.00,__reject_test__,2026-01-02,reject\n";out.close();rejects([&]{files.importCsv(invalid);});expenses.reload();check(expenses.matching(ExpenseFilter{}).size()==8,"Transaction rollback");
 }
 auth.logout();rejects([&]{files.exportCsv(folder/"signed-out.csv",ExpenseFilter{});});rejects([&]{files.importCsv(csv);});
 std::cout<<"CSV integration passed: quoting, roundtrip, generated IDs, invalid rows, transaction, weekly totals, budgets, file errors and auth gates.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
