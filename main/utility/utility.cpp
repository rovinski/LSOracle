#include <ot/timer/timer.hpp>

// Procedure: timing_to_sdc
void timing_to_sdc(const std::filesystem::path& timing, const std::filesystem::path& sdc) {
  
  // Open the timing file and sdc output
  std::ifstream tfs(timing);
  if(!tfs.good()) {
    OT_LOGF("can't open ", timing);
  }
  
  std::ofstream sfs(sdc);
  if(!sfs.good()) {
    OT_LOGF("can't open ", sdc);
  }

  std::string clock;
  std::string line, token, pin;
  std::array<std::array<float, ot::MAX_TRAN>, ot::MAX_SPLIT> value;

  OT_LOGI("converting .timing ", timing, " to .sdc ", sdc, " ...");

  size_t num_sdc {0};
  
  while(std::getline(tfs, line)) {
    
    std::istringstream ss(line);

    ss >> token;

    if(token == "clock") {
      OT_LOGI("generating create_clock ...");
      float period;
      ss >> clock >> period;
      sfs << "create_clock -period " << period << " -name " << clock 
          << " [get_ports " << clock << "]\n";
      ++num_sdc;
    }
    else if(token == "at") {
      OT_LOGI("generating set_input_delay ...");
      ss >> pin 
         >> value[ot::EARLY][ot::RISE] >> value[ot::EARLY][ot::FALL] 
         >> value[ot::LATE][ot::RISE] >> value[ot::LATE][ot::FALL];

      sfs << "set_input_delay " << value[ot::EARLY][ot::RISE] << " -min -rise"
                                << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs    << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_input_delay " << value[ot::EARLY][ot::FALL] << " -min -fall"
                                << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs    << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_input_delay " << value[ot::LATE][ot::RISE] << " -max -rise"
                                << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs    << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_input_delay " << value[ot::LATE][ot::FALL] << " -max -fall"
                                << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs    << " -clock " << clock;
      sfs << '\n';

      num_sdc += 4;
    }
    else if(token == "slew") {
      OT_LOGI("generating set_input_transition ...");
      ss >> pin
         >> value[ot::EARLY][ot::RISE] >> value[ot::EARLY][ot::FALL]
         >> value[ot::LATE][ot::RISE] >> value[ot::LATE][ot::FALL];
      
      sfs << "set_input_transition " << value[ot::EARLY][ot::RISE] << " -min -rise"
                                     << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs         << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_input_transition " << value[ot::EARLY][ot::FALL] << " -min -fall"
                                     << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs         << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_input_transition " << value[ot::LATE][ot::RISE] << " -max -rise"
                                     << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs         << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_input_transition " << value[ot::LATE][ot::FALL] << " -max -fall"
                                     << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs         << " -clock " << clock;
      sfs << '\n';

      num_sdc += 4;
    }
    else if(token == "rat") {
      OT_LOGI("generating set_output_delay ...");
      ss >> pin
         >> value[ot::EARLY][ot::RISE] >> value[ot::EARLY][ot::FALL]
         >> value[ot::LATE][ot::RISE] >> value[ot::LATE][ot::FALL];
      
      sfs << "set_output_delay " << value[ot::EARLY][ot::RISE] << " -min -rise"
                                 << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs     << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_output_delay " << value[ot::EARLY][ot::FALL] << " -min -fall"
                                 << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs     << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_output_delay " << value[ot::LATE][ot::RISE] << " -max -rise"
                                 << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs     << " -clock " << clock;
      sfs << '\n';
      
      sfs << "set_output_delay " << value[ot::LATE][ot::FALL] << " -max -fall"
                                 << " [get_ports " << pin << "]";
      if(!clock.empty()) sfs     << " -clock " << clock;
      sfs << '\n';

      num_sdc += 4;
    }
    else if(token == "load") {
      OT_LOGI("generating set_load ...");
      float v;
      ss >> pin >> v;
      sfs << "set_load -pin_load " << v << " [get_ports " << pin << "]\n";
      ++num_sdc;
    }
    else {
      OT_LOGF("unknown keyword ", token, " in ", timing);
    }
  }

  OT_LOGI("completed [", num_sdc, " sdc commands]");
}

// ------------------------------------------------------------------------------------------------

// Procedure: tau15_to_shell
void tau15_to_shell(const std::filesystem::path& tau15, const std::filesystem::path& shell) {

  std::filesystem::path early_celllib;
  std::filesystem::path late_celllib;
  std::filesystem::path spef;
  std::filesystem::path verilog;

  // open tau folder
  if(std::ifstream ifs(tau15); ifs) {
    ifs >> early_celllib >> late_celllib >> spef >> verilog; 
  }
  else {
    OT_LOGF("error in opening ", tau15);
  }

  // Convert the timing to sdc
  std::filesystem::path timing = tau15.stem().string() + ".timing";
  std::filesystem::path sdc = tau15.stem().string() + ".sdc";
  timing_to_sdc(timing, sdc);
  
  // Open shell output 
  std::ofstream sfs(shell);
  if(!sfs.good()) {
    OT_LOGF("can't open ", shell);
  }

  sfs << "read_celllib -early " << early_celllib.string() << '\n';
  sfs << "read_celllib -late " << late_celllib.string()  << '\n';
  sfs << "read_verilog " << verilog.string() << '\n';
  sfs << "read_spef " << spef.string() << '\n';
  sfs << "read_sdc "  << sdc.string() << '\n';
  sfs << "cppr -enable\n";

  // Open ops
  std::ifstream ofs(tau15.stem().string() + ".ops");
  if(!ofs.good()) {
    OT_LOGF("can't open ", tau15.stem().string() + ".ops");
  }
  
  sfs << ofs.rdbuf();  
}

// ------------------------------------------------------------------------------------------------

// Function: main
int main(int argc, char* argv[]) {

  ot::ArgParse app {"ot-utility"};

  std::vector<std::filesystem::path> t2s;
  std::vector<std::filesystem::path> o2s;

  app.add_option("--timing-to-sdc", t2s, "Convert a TAU15 timing file to sdc format")
     ->expected(2);

  app.add_option("--tau15-to-shell", o2s, "Convert a TAU15 bundle to a ot-shell file")
     ->expected(2);

  try {
    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& e) {
    return app.exit(e);
  }

  // ---- utility

  // convert timing to sdc
  if(!t2s.empty()) {
    timing_to_sdc(t2s[0], t2s[1]);
  }
  
  // conver ops to shell command
  if(!o2s.empty()) {
    tau15_to_shell(o2s[0], o2s[1]);
  }

  return 0;
}










