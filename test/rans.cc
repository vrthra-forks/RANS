#include <gflags/gflags.h>
#include <rans.hpp>

#include <iostream>
#include <fstream>
#include <string>

DEFINE_bool(dfa, false, "dump DFA as dot language.");
DEFINE_bool(matrix, false, "dump Matrix.");
DEFINE_bool(exmatrix, false, "dump Extended Matrix.");
DEFINE_bool(scc, false, "dump Strongly-connected-components of DFA.");
DEFINE_string(f, "", "obtain pattern from FILE.");
DEFINE_bool(i, false, "ignore case distinctions in both the REGEX and the input files..");
DEFINE_bool(minimizing, true, "minimizing DFA");
DEFINE_string(text, "", "print the value of given text on ANS.");
DEFINE_string(textf, "", "obtain text from FILE.");
DEFINE_string(check, "", "check wheter given text is acceptable or not.");
DEFINE_string(value, "", "print the text of given value on ANS.");
DEFINE_bool(verbose, false, "report additional informations.");
DEFINE_bool(syntax, false, "print RANS regular expression syntax.");
DEFINE_bool(utf8, false, "use utf8 as internal encoding.");
DEFINE_string(from, "", "convert the given value base from the given expression.");
DEFINE_string(into, "", "convert the given value base to the given expression.");
DEFINE_string(compress, "", "compress the given file (create '.rans' file, by default).");
DEFINE_string(decompress, "", "decompress the given file");
DEFINE_string(out, "", "output file name.");
DEFINE_bool(size, false, "print the size of the DFA.");
DEFINE_bool(amount, false, "print number of acceptable strings that has less than '--value' characters in length.");
DEFINE_int64(count, -1, "print number of acceptable strings that just has specified characters in length.");
DEFINE_bool(compression_ratio, false, "print asymptotic compression ratio [%].");
DEFINE_bool(frobenius_root, false, "print frobenius root of adjacency matrix.");
DEFINE_bool(frobenius_root2, false, "print frobenius root of adjacency matrix without linear algebraic optimization.");
DEFINE_bool(factorial, false, "make langauge as a factorial");
DEFINE_bool(tovalue, false, "convert the given text into the correspondence value");

void dispatch(const RANS&);
void set_filename(const std::string&, std::string&);

int main(int argc, char* argv[])
{
  google::SetUsageMessage(
      "RANS command line tool.\n"
      "Usage: rans REGEX [Flags ...] \n"
      "You can check RANS extended regular expression syntax via '--syntax' option."
                          );
  google::SetVersionString(std::string("build rev: ")+std::string(GIT_REV));
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_syntax) {
    std::cout << rans::SYNTAX;
    return 0;
  }
  
  std::string regex;
  if (!FLAGS_f.empty()) {
    std::ifstream ifs(FLAGS_f.data());
    ifs >> regex;
  } else if (argc > 1) {
    regex = argv[1];
  } else if (FLAGS_from.empty() || FLAGS_into.empty()) {
    std::cout << google::ProgramUsage() << std::endl;
    return 0;
  }
  if (FLAGS_text.empty() && !FLAGS_textf.empty()) {
    std::ifstream ifs(FLAGS_textf.data());
    if (ifs.fail()) {
      std::cerr << FLAGS_textf + " does not exists." << std::endl;
      return 0;
    }
    ifs >> FLAGS_text;
  }

  RANS::Encoding enc = FLAGS_utf8 ? RANS::UTF8 : RANS::ASCII;
  
  if (!FLAGS_from.empty() && !FLAGS_into.empty()) {
    RANS from(FLAGS_from, enc, FLAGS_factorial, FLAGS_i),
        to(FLAGS_into, enc, FLAGS_factorial, FLAGS_i);
    if (!from.ok() || !to.ok()) {
      std::cerr << from.error() << std::endl << to.error() << std::endl;
      return 0;
    }

    if (FLAGS_compression_ratio) {
      std::cout << from.compression_ratio(FLAGS_count, to) << std::endl;
    } else if (!FLAGS_text.empty()) {
      try {
        std::cout << to(from(FLAGS_text)) << std::endl;
      } catch (RANS::Exception& e) {
        std::cerr << e.what() << std::endl;
      }
    }
    return 0;
  }

  RANS r(regex, enc, FLAGS_factorial, FLAGS_i, FLAGS_minimizing);
  if (!r.ok()) {
    std::cerr << r.error() << std::endl;
    return 0;
  }

  if (FLAGS_dfa) std::cout << r.dfa();
  if (FLAGS_matrix) std::cout << r.adjacency_matrix();
  if (FLAGS_exmatrix) std::cout << r.extended_adjacency_matrix();
  if (FLAGS_scc) {
    for (std::size_t i = 0; i < r.scc().size(); i++) {
      for (std::set<std::size_t>::iterator iter = r.scc()[i].begin();
           iter != r.scc()[i].end(); ++iter) {
        std::cout << *iter << ", ";
      }
      std::cout << std::endl;
    }
  }
  if (!(FLAGS_dfa || FLAGS_matrix || FLAGS_exmatrix || FLAGS_scc)) {
    try {
      dispatch(r);
    } catch (RANS::Exception& e) {
      std::cerr << e.what() << std::endl;
    }
  }
  
  return 0;
}

void dispatch(const RANS& r) {
  if (FLAGS_frobenius_root) {
    std::cout << r.spectrum().root << std::endl;
  } else if (FLAGS_frobenius_root2) {
    std::cout << r.adjacency_matrix().frobenius_root() << std::endl;
  } else if (FLAGS_compression_ratio) {
    std::cout << r.compression_ratio(FLAGS_count) << std::endl;
  } else if (FLAGS_amount) {
    if (FLAGS_count < 0) {
      if (r.finite()) {
        std::cout << r.amount() << std::endl;
      } else {
        std::cerr << "there exists infinite acceptable strings." << std::endl;
      }
    } else {
      std::cout << r.amount(FLAGS_count) << std::endl;
    }
  } else if (FLAGS_count >= 0) {
    std::cout << r.count(FLAGS_count) << std::endl;
  } else if (!FLAGS_check.empty()) {
    if (r.accept(FLAGS_check)) {
      std::cerr << "text is acceptable." << std::endl;
    } else {
      std::cerr << "text is not acceptable." << std::endl;
    }
  } else if (!FLAGS_compress.empty()) {
    std::string text;
    std::ifstream ifs(FLAGS_compress.data(), std::ios::in | std::ios::binary);
    if (ifs.fail()) {
      std::cerr << FLAGS_decompress + " does not exists." << std::endl;
      return;
    }
    ifs >> text;

    if (FLAGS_out.empty()) {
      FLAGS_out = FLAGS_compress;
      FLAGS_out += ".rans";
    }

    std::ofstream ofs(FLAGS_out.data(), std::ios::out | std::ios::binary);
    try {
      std::string dst;
      ofs << r.compress(text, dst);
    } catch (const RANS::Exception &e) {
      std::cout << e.what() << std::endl;
    }
  } else if (!FLAGS_decompress.empty()) {
    std::ifstream ifs(FLAGS_decompress.data(), std::ios::in | std::ios::binary);
    if (ifs.fail()) {
      std::cerr << FLAGS_decompress + " does not exists." << std::endl;
      return;
    }

    std::istreambuf_iterator<char> first(ifs);
    std::istreambuf_iterator<char> last;
    std::string text(first, last);

    set_filename(FLAGS_decompress, FLAGS_out);
    if (FLAGS_out.empty()) return;
    std::ofstream ofs(FLAGS_out.data(), std::ios::out | std::ios::binary);
    try {
      std::string dst;
      ofs << r.decompress(text, dst);
    } catch (const RANS::Exception &e) {
      std::cerr << e.what() << std::endl;
    }
  } else if (FLAGS_size) {
    std::cout << "size of DFA: " << r.dfa().size() << std::endl;
  } else if (!FLAGS_value.empty()) {
    std::cout << r(RANS::Value(FLAGS_value)) << std::endl;
  } else if (!FLAGS_text.empty()) {
    std::cout << r(FLAGS_text) << std::endl;
  } else {
    std::string input;
    RANS num("[0-9]+");
    while (std::cin >> input) {
      try {
        if (FLAGS_tovalue) {
          std::cout << r(input) << std::endl;
        } else {
          if (num.accept(input)) std::cout << r(RANS::Value(input)) << std::endl;
          else std::cerr << "invalid value: " << input << std::endl;
        }
      } catch(const RANS::Exception& e) {
        std::cerr << e.what() << std::endl;
      }
    }
  }
}

void set_filename(const std::string& src, std::string& dst)
{
  if (!dst.empty()) return;

  static const std::string suffix = ".rans";

  if (src.length() > suffix.length() &&
      src.substr(src.length() - suffix.length(), std::string::npos) == suffix) {
    dst = src.substr(0, src.length() - suffix.length());
  } else {
    std::string input = "dummy";
    do {
      switch (input[0]) {
        case 'y': case 'A': dst = src; return;
        case 'n': case 'N': return;
        case 'r':
          std::cout << "new name: ";
          std::cin >> dst;
          return;
        default:
          std::cout << "replace " << src << "? [y]es, [n]o, [A]ll, [N]one, [r]ename: ";
      }
    } while (std::cin >> input);
  }
}
