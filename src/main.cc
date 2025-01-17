#include <stdio.h>
#include <time.h> 
#include <iostream>
#include <iomanip>
#include <fstream>
#include <iosfwd>
#include <sstream>
#include <signal.h>
#include <unistd.h>
#include <algorithm>
#include "ToolConfig.hh"
#include "ReadCNF.hh"
#include "auxiliary.hh"
#include "BackboneInformation.hh"
#include "Worker.hh"
#include "UpperBound.hh"
#include "CoreBased.hh"
#include "UpperBoundProg.hh"

using std::cout;
using std::cin;
using std::setprecision;
using namespace minibones;

bool            print_help=false; 
ofstream        output_file;
ToolConfig      config;
Worker          *pworker=NULL;
UpperBound      *pupperbound=NULL;
CoreBased       *pcorebased=NULL;
UpperBoundProg  *pupperbound_prog=NULL;
Range           range;
bool            instance_sat = false;

void print_usage();
void print_header();
void print_header(ToolConfig& config);
bool parse_options(int argc, char** argv, /*out*/ToolConfig& config);
void print_backbone(const BackboneInformation& worker, const Range& range, ToolConfig& config, ostream& output);
int run_worker(ToolConfig& config, ostream& output);
int run_upper_bound(ToolConfig& config, ostream& output);
int run_core_based(ToolConfig& config, ostream& output);
int run_upper_bound_prog(ToolConfig& config, ostream& output);
void register_sig_handlers();
void initialize_picosat();
char *_strdup(const char *s);

int main(int argc, char** argv) {
  print_header();
  register_sig_handlers();
#ifndef EXPERT
  // prepare nonexpert options
  int nargc = 0;
  char* nargv[20];
  nargv[nargc++] = _strdup(argv[0]);
  nargv[nargc++] = _strdup("-e");
  nargv[nargc++] = _strdup("-i");
  nargv[nargc++] = _strdup("-c");
  nargv[nargc++] = _strdup("100");
  nargv[nargc++] = argc>=2 ? _strdup(argv[1]) : _strdup("-");
  if (argc>2) {
    cerr<<"ERROR: ingoring some options after FILENAME"<<std::endl;
    return 100;
  }
  parse_options(nargc, nargv, config);
  while (nargc--) delete[] nargv[nargc];
#else
  cout<<"c WARNING: running in the EXPERT mode, I'm very stupid without any options."<<std::endl;
  if (!parse_options(argc, argv, config) || print_help) {
     print_usage();
     return 100;
  }
#endif

  print_header(config);
  ostream& output=std::cout;
  int return_value;
  if (config.get_self_test()) {
    cout << "running self test" << endl;
    const bool tests_okay =  true;// test_all();
    cout << "self test: " << (tests_okay ? "OK" : "FAIL") << endl;
    return_value = tests_okay ? 0 : 20;
  } else if (config.get_use_upper_bound()) {
    return_value = run_upper_bound(config, output);
  } else if (config.get_use_core_based()) {
    return_value = run_core_based(config, output);
  } else if (config.get_use_upper_bound_prog()) {
    return_value = run_upper_bound_prog(config, output);
  } else {
    return_value = run_worker(config, output);
  }
  if (return_value==10||return_value==20) 
    cout << "i complete" << endl;
  return return_value;
}

Reader* make_reader(string flafile) {
  gzFile ff=Z_NULL;
  if (flafile.size()==1 && flafile[0]=='-') {
    return new Reader(std::cin);
  } else {
    ff = gzopen(flafile.c_str(), "rb");
    if (ff==Z_NULL) {
      cerr << "Unable to open file: " << flafile << endl;
      exit(100);
    }
    return new Reader(ff);
  }
  assert(0);
  return NULL;
}

int run_upper_bound(ToolConfig& config, ostream& output) {
  //read input
  Reader* fr = make_reader(config.get_input_file_name());  
  ReadCNF reader(*fr);
  reader.read();

  //determine which part of variables to compute backbone of
  range=Range(1,reader.get_max_id());
  output << "c range: "<<range.first<<"-"<<range.second<<endl;
  pupperbound = new UpperBound(config,output, reader.get_max_id(), reader.get_clause_vector());
  UpperBound& upperbound=*pupperbound;
  if (!upperbound.initialize()) {//unsatisfiable
    config.prefix(output) << "instance unsatisfiable" << endl;
    output << "s 0" << endl;
    delete pupperbound; 
    return 20;
  }
  config.prefix(output) << "instance satisfiable" << endl << flush;
  instance_sat = true;
  //run upperbound
  upperbound.run();
  config.prefix(output) << "computation completed " << endl;
  cout << "i sc:" << upperbound.get_solver_calls() << endl;
  cout << "i st:" << upperbound.get_solver_time() << endl;
  cout << "i ast:" << setprecision(2) << (upperbound.get_solver_time()/(double)upperbound.get_solver_calls()) << endl;
  //print results
  print_backbone(upperbound, range, config, output);
  delete pupperbound; 
  delete fr;
  return 10;
}


int run_core_based(ToolConfig& config, ostream& output) {
  //read input
  Reader* fr = make_reader(config.get_input_file_name());
  ReadCNF reader(*fr);
  try {
    reader.read();
  } catch (const ReadCNFException& e) {
    cerr<<"ERROR while reading: "<<e.what()<<"."<<endl;
    return 100;
  }

  //determine which part of variables to compute backbone of
  range=Range(1,reader.get_max_id());
  output << "c range: "<<range.first<<"-"<<range.second<<endl;
  pcorebased = new CoreBased(config, output, reader.get_max_id(), reader.get_clause_vector());
  CoreBased& corebased=*pcorebased;
  if (!corebased.initialize()) {//unsatisfiable
    config.prefix(output) << "instance unsatisfiable" << endl;
    output << "s 0" << endl;
    delete pcorebased; 
    return 20;
  }
  instance_sat = true;
  config.prefix(output) << "instance satisfiable" << endl << flush;
  //run corebased
  cout << "c CoreBased algorithm, chunk: " << config.get_chunk_size()  << endl;
  corebased.run();
  config.prefix(output) << "computation completed " << endl;
  cout << "i sc:" << corebased.get_solver_calls() << endl;
  cout << "i st:" << corebased.get_solver_time() << endl;
  cout << "i ast:" << setprecision(2) << (corebased.get_solver_time()/(double)corebased.get_solver_calls()) << endl;
  //print results
  print_backbone(corebased, range, config, output);
  delete pcorebased; 
  delete fr;
  return 10;
}


int run_upper_bound_prog(ToolConfig& config, ostream& output) {
  //read input
  Reader* fr = make_reader(config.get_input_file_name());  
  ReadCNF reader(*fr);
  reader.read();

  //determine which part of variables to compute backbone of
  range=Range(1,reader.get_max_id());
  output << "i range: "<<range.first<<"-"<<range.second<<endl;
  pupperbound_prog = new UpperBoundProg(config, output, reader.get_max_id(), reader.get_clause_vector());
  UpperBoundProg& upperbound_prog=*pupperbound_prog;
  if (!upperbound_prog.initialize()) {//unsatisfiable
    config.prefix(output) << "instance unsatisfiable" << endl;
    output << "s 0" << endl;
    delete pupperbound_prog; 
    return 20;
  }
  instance_sat = true;
  //run upperbound
  upperbound_prog.run();
  config.prefix(output) << "computation completed " << endl;
  cout << "i sc:" << upperbound_prog.get_solver_calls() << endl;
  cout << "i st:" << upperbound_prog.get_solver_time() << endl;
  cout << "i ast:" << setprecision(2) << (upperbound_prog.get_solver_time()/(double)upperbound_prog.get_solver_calls()) << endl;
  //print results
  print_backbone(upperbound_prog, range, config, output);
  delete pupperbound_prog; 
  delete fr;
  return 10;
}

int run_worker(ToolConfig& config, ostream& output) {
  //read input
  Reader* fr = make_reader(config.get_input_file_name());
  ReadCNF reader(*fr);
  try {
    reader.read();
  } catch (const ReadCNFException& e) {
    cerr<<"ERROR while reading: "<<e.what()<<"."<<endl;
    return 100;
  }

  //determine which part of variables to compute backbone of
  range=Range(1,reader.get_max_id());
  output << "c range: "<<range.first<<"-"<<range.second<<endl;
  pworker = new Worker(config,output, reader.get_max_id(), reader.get_clause_vector(), range);
  Worker& worker=*pworker;
  if (!worker.initialize()) {//unsatisfiable
    config.prefix(output) << "instance unsatisfiable" << endl;
    output << "s 0" << endl;
    delete pworker; 
    return 20;
  }
  instance_sat = true;
  //run worker
  worker.run();
  config.prefix(output) << "computation completed " << endl;
  cout << "i sc:" << worker.get_solver_calls() << endl;
  cout << "i st:" << worker.get_solver_time() << endl;
  cout << "i ast:" << setprecision(2) << (worker.get_solver_time()/(double)worker.get_solver_calls()) << endl;
  //print results
  print_backbone(worker, range, config, output);
  delete pworker; 
  return 10;
}

void print_backbone(const BackboneInformation& worker, const Range& range, ToolConfig& config, ostream& output) {
  output << "s 1" << endl;
  output << "v ";
  size_t counter = 0;
  for (Var v = range.first; v <= range.second; ++v) {
    if (worker.is_backbone(v)) {
      ++counter;
      output << ' ';
      output << (worker.backbone_sign(v) ? "" : "-") << v;
    }
  }
  output << endl;
  const float percentage = 100.0 * (float)counter / (float)(range.second - range.first + 1);
  config.prefix(output) << "backbone size: "
                        << counter << " "
                        << setprecision(2) << percentage << "% of range"
                        << endl;
}

//jpms:bc
/*----------------------------------------------------------------------------*\
 * Purpose: Print runtime header when executing DBBones.
\*----------------------------------------------------------------------------*/
//jpms:ec

void print_header(ToolConfig& config) {
  cout_pref<<"contributors: "<<contribs<<endl;
  cout_pref<<"instance: " << config.get_input_file_name()<<endl;
  cout_pref<<endl;
}

 bool parse_options(int argc, char** argv, ToolConfig& config) {
  opterr = 0;
  int c;
  while ((c = getopt(argc, argv, "eobhikmpuc:rl")) != -1) {
    switch (c) {
    case 'h':
      print_help=true;
      break;
    case 'i':
      config.set_backbone_insertion(1);
      break;
    case 'o':
      config.set_use_cores(true);
      break;
    case 'e':
      config.set_use_core_based(true);
      break;
    case 'p':
      config.set_use_upper_bound_prog(true);
      break;
    case 'u':
      config.set_use_upper_bound(true);
      break;
    case 'r':
      config.set_rotatable_pruning(true);
      break;
    case 'b':
      config.set_use_variable_bumping(1);
      break;
    case 'k':
      config.set_use_chunk_keeping(1);
      break;
    case 'm':
      config.set_use_random_chunks(1);
      break;
    case 'c':
      config.set_chunk_size(atoi(optarg));
      break;
    case 'l':
      config.set_scdc_pruning();
      break;
    case '?':
      if (optopt == 'c')
        fprintf (stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt)) fprintf (stderr, "Unknown option `-%c'.\n", optopt);
      else fprintf (stderr,"Unknown option character `\\x%x'.\n",optopt);
      return false;
    default:
      return false;
    }
  }

  if (!print_help && (optind >= argc)) {
     cerr << "ERROR: file name expected" << endl;
     return false;
  } else {
    config.set_self_test(false);
    if(!print_help) config.set_input_file_name(argv[optind]);
  }
  return true;
}


//jpms:bc
/*----------------------------------------------------------------------------*\
 * Purpose: Handler for external signals, namely SIGHUP and SIGINT.
\*----------------------------------------------------------------------------*/
//jpms:ec

static void SIG_handler(int signum);
static void finishup();

void register_sig_handlers() {
  signal(SIGHUP,SIG_handler);
  signal(SIGINT,SIG_handler);
  signal(SIGQUIT,SIG_handler);
  signal(SIGTERM,SIG_handler);
  signal(SIGABRT,SIG_handler);
  signal(SIGALRM,SIG_handler);
}

// Generic handler, for all process termination signals that can be caught
static void SIG_handler(int signum) {
  string signame;
  switch (signum) {
    //case 6:  signame = "SIGABRT"; break;
  case 1:  signame = "SIGHUP"; break;
  case 2:  signame = "SIGINT"; break;
  case 3:  signame = "SIGQUIT"; break;
  case 9:  signame = "SIGKILL"; break;
  case 15: signame = "SIGTERM"; break;
  default: cout << "Invalid signal received...\n"; exit(2); break;
  }
  cout << "dbbones received external signal " << signame << ". " << endl;
  cout << "Terminating and printing whatever computed so far." << endl;
  finishup();
}

static void finishup() {
  if (instance_sat) {
    if (pworker) print_backbone (*pworker , range, config, cout);
    if (pupperbound) print_backbone (*pupperbound, range, config, cout);
    if (pupperbound_prog) print_backbone (*pupperbound_prog, range, config, cout);
  } else cout << "s 0" << endl;
  //  if (pworker) delete pworker;
  exit(instance_sat? 10 : 20);
}


void print_header() {
  cerr<<"c minibones, a tool for backbone computation"<<std::endl;
  cerr<<"c (C) 2012 Mikolas Janota, mikolas.janota@gmail.com"<<std::endl;
}

void print_usage() {
  cout << "USAGE"<<"\tminibones <file name>"<<endl;
  cout<<"    -l ... lifting"<<endl;
  cout<<"    -b ... variable activity bumping"<<endl;
  cout<<"    -r ... rotatable variables"<<endl;
  cout<<"    -e ... corE based approach"<<endl;
  cout<<"    -u ... upper bound"<<endl;
  cout<<"    -c S ... chunk of size S (requires -u or -e)"<<endl;
  cout<<"    -m ... rando*m* content of  chunks (requires -u)"<<endl;
  cout<<"    -i ... insertion of the backbone into the formula after it has been found (this is default in lower bound)"<<endl;
  cout<<"    -k ... which *k*eeps a literal in the chunk until it is decided whether it is  a backbone or not (requires -u)."<<endl;
  cout<<"    -p ... programming chunks (one big clause is programmed to represent different chunks)"<<endl;
  cout << "NOTES:"<<endl;
  cout <<"   if filename is '-', instance is read from the standard input" << endl;
}

char *_strdup(const char *s) {
    size_t sz = strlen(s) + 1;
    char *p = new char[sz];
    while (sz--) p[sz] = s[sz];
    return p;
}
