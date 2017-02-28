#ifndef WATERZ_BIN_QUEUE_H__
#define WATERZ_BIN_QUEUE_H__

#include <exception>
#include <queue>

class BinQueueOutOfBoundsException : public std::exception {

	public:

		BinQueueOutOfBoundsException(int i, int n) {

			_ss << "Invalid bin " << i << ", not in range [0," << n << ")";
		}

		const char* what() {

			return _ss.str().c_str();
		}

	private:

		std::stringstream _ss;
};

/**
 * A priority queue sorting elements from smallest to largest.
 *
 * Assumes that scores are given in the interval [0,1]. Scores are discretized 
 * and placed in N bins.
 */
template <typename T, typename ScoreType, int N=256>
class BinQueue {

public:

	BinQueue() :
		_minBin(-1) {}

	void push(const T& element, ScoreType score) {

		int i = scoreToIndex(score);

		_bins[i].push(element);
		if (_minBin == -1)
			_minBin = i;
		else
			_minBin = std::min(i, _minBin);
	}

	const T& top() const {

		return _bins[_minBin].front();
	}

	void pop() {

		_bins[_minBin].pop();

		if (_bins[_minBin].empty()) {

			// find next non-empty bin
			for (;_minBin < N; _minBin++)
				if (!_bins[_minBin].empty())
					return;

			// queue is empty
			_minBin = -1;
		}
	}

	bool empty() const {

		return (_minBin == -1);
	}

	size_t size() const {

		if (empty())
			return 0;

		size_t sum = 0;
		for (int i = _minBin; i < N; i++)
			sum += _bins[i].size();

		return sum;
	}

private:

	inline int scoreToIndex(ScoreType score) {

		int i = (int)(score*(N-1));

		if (i < 0 || i >= N)
			throw BinQueueOutOfBoundsException(i, N);

		return i;
	}

	std::queue<T> _bins[N];

	// smallest non-empty bin
	int _minBin;
};

#endif // WATERZ_BIN_QUEUE_H__

