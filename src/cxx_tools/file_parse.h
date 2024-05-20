/*
 * File: file_parse.h
 * Author: Sean Gallogly
 * Created on: 10/01/2022
 *
 * Description:
 *     This is the interface file for reading in a build file. It includes the
 *     structures, enums, and tables necessary to tokenize, lex, and parse a
 * build file. This file is more of a workflow and less of a class interface
 * file in that the end result of this process is to obtain a structure of type
 *     parsed_file that contains all of the parameters that we need to build a
 *     simulation from scratch, akin to "generating" a new rabbit on the fly.
 */
#ifndef FILE_PARSE_H_
#define FILE_PARSE_H_

#include <cstdint>
#include <map>
#include <string>
#include <utility> /* for std::pair */
#include <vector>

typedef struct {
  uint32_t num_trials;
  std::string *trial_names;
  uint32_t *use_css;
  uint32_t *cs_onsets;
  uint32_t *cs_lens;
  float *cs_percents;
  uint32_t *use_uss;
  uint32_t *us_onsets;
} trials_data;

/*
 * a section within an input file that includes the label of the section
 * and the dictionary of parameters in that section
 *
 */
typedef struct {
  std::map<std::string, std::string> param_map;
} parsed_var_section;

typedef struct {
  std::map<std::string, std::map<std::string, std::string>> trial_map;
  std::map<std::string, std::vector<std::pair<std::string, std::string>>>
      block_map;
  std::vector<std::pair<std::string, std::string>>
      session; // <-- pairs of block identifier and number of blocks
} parsed_trial_section;

typedef struct {
  parsed_trial_section parsed_trial_info;
  std::map<std::string, parsed_var_section> parsed_var_sections;
} parsed_sess_file;

/*
 * Description:
 *     takes a lexed file reference l_file and parses it, i.e. takes each lexeme
 *     and adds it to the correct entry in either
 * parsed_sess_file.parsed_trial_info or parsed_sess_file.parsed_var_sections
 *
 */
// void parse_lexed_sess_file(lexed_file &l_file, parsed_sess_file &s_file);

/*
 * Description:
 *     takes in parsed_session_file 'from_s_file' and copies its data into
 * 'to_s_file'
 */
void cp_parsed_sess_file(parsed_sess_file &from_s_file,
                         parsed_sess_file &to_s_file);

/*
 * Description:
 *     allocates memory for the arrays within reference to trials_data td.
 *     Caller is responsible for the memory that is allocated in this function.
 *     (blah blah blah, fancy talk for if you call this function, you will want
 *      to call delete_trials_data at a later point to prevent memory leak)
 *
 */
void allocate_trials_data(trials_data &td, uint32_t num_trials);

/*
 * Description:
 *     initializes elements of arrays in td according to parsed trial
 * definitions in pt_section. NOTE: "allocate_trials_data" must be run before
 * this function is called.
 *
 */
void initialize_trials_data(trials_data &td, parsed_trial_section &pt_section);

/*
 * Description:
 *     translates trials section information in parsed sess file into td.
 *     td follows the structure of arrays (SoA) paradigm for efficiency in
 *     member element access. Data in td is used later in Control::runSession
 *
 */
void translate_parsed_trials(parsed_sess_file &s_file, trials_data &td);

/*
 * Description:
 *     deallocates memory for the data members of td. NOTE: allocate_trials_data
 *     must be called before this function is called.
 *
 */
void delete_trials_data(trials_data &td);

/*
 * Description of following 4 functions:
 *      following are used to print the given type to the console like
 *      you would use the stream insertion operator for atomic types and stl
 *      types for which an operator overload exists.
 *
 * Example Usage:
 *
 *     std::cout << t_file << std::endl;
 */

std::ostream &operator<<(std::ostream &os, parsed_sess_file &s_file);

#endif /* FILE_PARSE_H_ */
