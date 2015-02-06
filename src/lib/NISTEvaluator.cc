#include <mteval/NISTEvaluator.h>

#include <mteval/utils.h>

#include <cmath>

using namespace std;

namespace MTEval {

NISTEvaluator::NISTEvaluator(const vector<EvaluatorParam> & params)
    : Evaluator(params)
    , ngram_(5) {

    for (auto & p : params) {
        if (p.name == "ngram") ngram_ = p.int_val;
    }

    resetCumulative();
}

NISTEvaluator::~NISTEvaluator() {}

void NISTEvaluator::prepare(const Sentence & reference, const Sentence & hypothesis) {
    int len_ref = reference.size();
    for (int n = 0; n < ngram_; ++n) {
        for (int k = 0; k + n < len_ref; ++k) {
            ++freq_[Utility::makeNGram(reference, k, n + 1)];
        }
    }

    freq_[Sentence()] += len_ref; // for context of unigram
}

void NISTEvaluator::calculate(const Sentence & reference, const Sentence & hypothesis) {
    if (!freq_[Sentence()]) throw runtime_error("evaluator is not prepared");

    int len_ref = reference.size();
    int len_hyp = hypothesis.size();
    total_len_ref_ += len_ref;
    total_len_hyp_ += len_hyp;
    
    // calculate averaged information weights

    map<Sentence, int> possible;
    int max_n = len_hyp < ngram_ ? len_hyp : ngram_;

    for (int n = 0; n < max_n; ++n) {
        denominators_[n] += len_hyp - n;

        for (int k = 0; k + n < len_ref; ++k) {
            ++possible[Utility::makeNGram(reference, k, n + 1)];
        }

        for (int k = 0; k + n < len_hyp; ++k) {
            Sentence ngram = Utility::makeNGram(hypothesis, k, n + 1);
            Sentence context = Utility::makeNGram(hypothesis, k, n);
            auto it = possible.find(ngram);
            if (it != possible.end() && it->second > 0) {
                --it->second;
                numerators_[n] += log(static_cast<double>(freq_[context]) / freq_[ngram]);
            }
        }
    }
}

double NISTEvaluator::getCumulative() const {
    
    // beta = -ln2 / (ln3 - ln2)^2
    // note: bp = 0.5 when |hyp|/E[|ref|] == 2/3
    constexpr double log_2 = log(2.0);
    constexpr double base = log(3.0) - log_2;
    constexpr double beta = -log_2 / (base * base);

    // calculate n-gram score
    double np = 0.0;
    for (int n = 0; n < ngram_; ++n) {
        if (denominators_[n] > 0.0) {
            np += numerators_[n] / (denominators_[n] * log_2);
        }
    }

    // calculate brevity penalty
    double bp = 1.0;
    if (total_len_ref_ > 0.0) {
        bp = static_cast<double>(total_len_hyp_) / total_len_ref_;
        bp = log(bp < 1.0 ? bp : 1.0);
        bp = exp(beta * bp * bp);
    }

    // calculate final score
    return np * bp;
}

void NISTEvaluator::resetCumulative() {
    numerators_.assign(ngram_, 0.0);
    denominators_.assign(ngram_, 0);
    total_len_ref_ = 0;
    total_len_hyp_ = 0;
}

string NISTEvaluator::getName() const {
    return "NIST";
}

} // namespace MTEval

