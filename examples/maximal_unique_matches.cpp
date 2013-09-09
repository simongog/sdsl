#include <sdsl/csa_wt.hpp>
#include <sdsl/vectors.hpp>
#include <sstream>
#include <iostream>

using namespace sdsl;

/* 
   This is an algorithm for computing maximal unique matches between two strings.
   Takes as input the BWT of A#B (string A concatenated with B adding # in between),
   and the BWT of reverse of A#B. It builds then two FM-indexes and an indicator vector. 
   The algorithm backtracks with all alphabet symbols, but visits only virtual suffix tree
   nodes. See details in 

   Djamal Belazzougui, Fabio Cunial, Juha Kärkkäinen, Veli Mäkinen: 
   Versatile Succinct Representations of the Bidirectional Burrows-Wheeler Transform. 
   ESA 2013: 133-144.
   
   This is not a faithful implementation of the O(n log sigma) algorithm given there, since
   operations are computed using trivial scans with all sigma alphabet symbols. 
   However, this version probably works fine with DNA.
*/


/*
  Index that tells from which string a suffix comes from.
 */
template <class Csa>
class SuffixIndex {
private:
  bit_vector v;
  rank_support_v<> index;
public:
  SuffixIndex(Csa &fm_index,
              size_t a_length,
              size_t b_length)
    : v(a_length + 1 + b_length + 1, 1), index() {
    size_t lb = 0;
    size_t ub = fm_index.size() - 1;
    algorithm::backward_search(fm_index, 0, fm_index.size() - 1, '#', lb, ub);
    if (lb < ub) {
      throw;
    }
    for (size_t i = 0; i < a_length; i++) {
      lb = fm_index.psi(lb);
      v[lb] = 0;
    }
    index.init(&v);
  }

  size_t rank_one(size_t lb, size_t ub)
  {
    return index.rank(ub) - index.rank(lb);
  }

  size_t rank_zero(size_t lb, size_t ub)
  {
    return (ub - lb) - rank_one(lb, ub);
  }
};

class SearchTreeNode {
public:
  size_t sp;
  size_t ep;
  size_t spr;
  size_t epr;
  size_t depth;
  SearchTreeNode(size_t _sp, size_t _ep,
                 size_t _spr, size_t _epr,
                 size_t _depth)
    : sp(_sp), ep(_ep), spr(_spr), epr(_epr), depth(_depth) {
  };
};

template <class Csa>
void construct_fm_index(Csa &csa, std::string &bwt_path)
{
  int_vector_file_buffer<8> buf;
  buf.load_from_plain(bwt_path.c_str());
  csa = Csa(buf);
  std::cerr << "Index construction complete, index requires ";
  std::cerr << util::get_size_in_mega_bytes(csa) << " MiB." << std::endl;
}

void parse_args(int argc, char **argv,
                std::string &catfile_path, std::string &revcatfile_path,
                size_t &a_length, size_t &b_length,
                size_t &minlen, size_t &maxlen)
{
  if(argc < 7) {
    std::cerr << "usage: " << argv[0];
    std::cerr<< " A#B.bwt revA#revB.bwt Alength Blength minlen maxlen";
    std::cerr << std::endl;
    std::cerr << "      If maxlen=0, then no maximum limit is used.";
    std::cerr << std::endl;
    throw;
  }

  catfile_path = argv[1];
  revcatfile_path = argv[2];

  std::stringstream a_lengthstr(argv[3]);
  std::stringstream b_lengthstr(argv[4]);
  std::stringstream minstr(argv[5]);
  std::stringstream maxstr(argv[6]);
  a_lengthstr >> a_length;
  b_lengthstr >> b_length;
  minstr >> minlen;
  maxstr >> maxlen;
  if (a_lengthstr.fail() || b_lengthstr.fail() || minstr.fail() || maxstr.fail()) {
    std::cerr << "failed parsing arguments" << std::endl;
    throw;
  }
}

template <class Csa>
void output_mum(Csa &fm_index, size_t a_length, SearchTreeNode &s) {
  size_t sp = fm_index[s.sp];
  size_t ep = fm_index[s.ep];
  if (sp < a_length && ep >= a_length) {
    std::cout << sp << ",";
    std::cout << (ep - a_length - 1) << ",";
    std::cout << s.depth << std::endl;
  } else if (ep < a_length && sp >= a_length) {
    std::cout << ep << ",";
    std::cout << (sp - a_length - 1) << ",";
    std::cout << s.depth << std::endl;
  }
}

int main(int argc, char **argv) {
  std::string catfile_path;
  std::string revcatfile_path;
  size_t a_length;
  size_t b_length;
  size_t minlen;
  size_t maxlen;
  parse_args(argc, argv, catfile_path, revcatfile_path,
             a_length, b_length, minlen, maxlen);

  typedef csa_wt<wt<>, 2, 100000> ForwardIndex;
  typedef csa_wt<wt<>, 100000, 100000> BackwardIndex;
  ForwardIndex forward_index;
  BackwardIndex backward_index;
  construct_fm_index<ForwardIndex>(forward_index, catfile_path);
  construct_fm_index<BackwardIndex>(backward_index, revcatfile_path);

  SuffixIndex<ForwardIndex> suffix_index(forward_index, a_length, b_length);

  //************COMPUTING THE MUMS*******************
  std::vector<size_t> spc(forward_index.sigma);
  std::vector<size_t> epc(forward_index.sigma);
  std::vector<size_t> spcr(forward_index.sigma);
  std::vector<size_t> epcr(forward_index.sigma);
  std::stack<SearchTreeNode> stc;
  stc.push(SearchTreeNode(0, forward_index.size() - 1,
                          0, backward_index.size() - 1,
                          0));
   while (!stc.empty()) {
      SearchTreeNode s = stc.top();
      stc.pop();

      if (s.ep - s.sp + 1 < 2 ||
          suffix_index.rank_one(s.sp, s.ep + 1) == 0 ||
          suffix_index.rank_zero(s.sp, s.ep + 1) == 0) {
        continue; // no MUM in this branch
      }
      
      bool left_maximal = true;
      bool right_maximal = true;
      for (size_t i = 0; i < forward_index.sigma; i++) {
        char c = (forward_index.comp2char)[i];
        size_t l = algorithm::backward_search(forward_index,
                                              s.sp, s.ep,
                                              c,
                                              spc[i], epc[i]);
        size_t rl = algorithm::backward_search(backward_index,
                                               s.spr, s.epr,
                                               c,
                                               spcr[i], epcr[i]);
        if (l == s.ep - s.sp + 1) {
          left_maximal = false;
        }
        if (rl == s.epr - s.spr + 1) {
          right_maximal = false;
        }
      }
      
      if (!right_maximal) {
        continue; // extending to left cannot make this branch right-maximal
      }

      if ((left_maximal || s.depth == maxlen) &&
          s.depth >= minlen &&
          s.ep - s.sp + 1 == 2) {
        output_mum(forward_index, a_length, s);
      }
      
      if (maxlen == 0 || s.depth < maxlen) {
        size_t sum = 0;
        size_t largest_interval = 0;
        size_t largest_interval_i = 0;
        for (size_t i = 0; i < forward_index.sigma; i++) {
          spcr[i] = s.spr + sum;
          epcr[i] = spcr[i] + epc[i] - spc[i];
          sum += epc[i] - spc[i] + 1;
          if (epc[i] - spc[i] + 1 > largest_interval) {
            largest_interval = epc[i] - spc[i] + 1;
            largest_interval_i = i;
          }
        }
        stc.push(SearchTreeNode(spc[largest_interval_i], epc[largest_interval_i],
                                spcr[largest_interval_i], epcr[largest_interval_i],
                                s.depth + 1));
        for (size_t i = 0; i < largest_interval_i; i++) {
          stc.push(SearchTreeNode(spc[i], epc[i],
                                  spcr[i], epcr[i],
                                  s.depth + 1));
        }
        for (size_t i = largest_interval_i + 1; i < forward_index.sigma; i++) {
          stc.push(SearchTreeNode(spc[i], epc[i],
                                  spcr[i], epcr[i],
                                  s.depth + 1));
        }
      }
   }
}
