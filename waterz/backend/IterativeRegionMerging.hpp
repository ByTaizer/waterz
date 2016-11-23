#ifndef ITERATIVE_REGION_MERGING_H__
#define ITERATIVE_REGION_MERGING_H__

#include <iostream>
#include <vector>
#include <map>
#include <queue>
#include <cassert>

#include "RegionGraph.hpp"

template <typename NodeIdType, typename ScoreType>
class IterativeRegionMerging {

public:

	typedef RegionGraph<NodeIdType>              RegionGraphType;
	typedef typename RegionGraphType::EdgeType   EdgeType;
	typedef typename RegionGraphType::EdgeIdType EdgeIdType;

	/**
	 * Create a region merging for the given initial RAG.
	 */
	IterativeRegionMerging(RegionGraphType& initialRegionGraph) :
		_regionGraph(initialRegionGraph),
		_edgeScores(initialRegionGraph),
		_deleted(initialRegionGraph),
		_stale(initialRegionGraph),
		_edgeQueue(EdgeCompare(_edgeScores)),
		_mergedUntil(0) {}

	/**
	 * Merge a RAG with the given edge scoring function until the given threshold.
	 */
	template <typename EdgeScoringFunction>
	void mergeUntil(
			EdgeScoringFunction& edgeScoringFunction,
			ScoreType threshold) {

		if (threshold <= _mergedUntil) {

			std::cout << "already merged until " << threshold << ", skipping" << std::endl;
			return;
		}

		// compute scores of each edge not scored so far
		if (_mergedUntil == 0) {

			std::cout << "computing initial scores" << std::endl;

			for (EdgeIdType e = 0; e < _regionGraph.edges().size(); e++)
				scoreEdge(e, edgeScoringFunction);
		}

		std::cout << "merging until " << threshold << std::endl;

		// while there are still unhandled edges
		while (_edgeQueue.size() > 0) {

			// get the next cheapest edge to merge
			EdgeIdType next = _edgeQueue.top();
			ScoreType score = _edgeScores[next];

			// stop, if the threshold got exceeded
			// (also if edge is stale or got deleted, as new edges can only be 
			// more expensive)
			if (score >= threshold) {

				std::cout << "threshold exceeded" << std::endl;
				break;
			}

			_edgeQueue.pop();

			if (_deleted[next])
				continue;

			if (_stale[next]) {

				// if we encountered a stale edge, recompute it's score and 
				// place it back in the queue
				ScoreType newScore = scoreEdge(next, edgeScoringFunction);
				_stale[next] = false;
				assert(newScore >= score);

				continue;
			}

			mergeRegions(next, edgeScoringFunction);
		}

		_mergedUntil = threshold;
	}

	/**
	 * Get the segmentation corresponding to the current merge level.
	 *
	 * The provided segmentation has to hold the initial segmentation, or any 
	 * segmentation created by previous calls to extractSegmentation(). In other 
	 * words, it has to hold IDs that have been seen before.
	 */
	template <typename SegmentationVolume>
	void extractSegmentation(SegmentationVolume& segmentation) {

		for (std::size_t i = 0; i < segmentation.num_elements(); i++)
			segmentation.data()[i] = getRoot(segmentation.data()[i]);
	}

private:

	/**
	 * Compare two edges based on their score. To be used in the priority queue.
	 */
	class EdgeCompare {

	public:

		EdgeCompare(const typename RegionGraphType::template EdgeMap<ScoreType>& edgeScores) :
			_edgeScores(edgeScores) {}

		bool operator()(EdgeIdType a, EdgeIdType b) {

			if (_edgeScores[a] == _edgeScores[b])
				return a > b;

			return _edgeScores[a] > _edgeScores[b];
		}

	private:

		const typename RegionGraphType::template EdgeMap<ScoreType>& _edgeScores;
	};

	/**
	 * Merge regions a and b.
	 */
	template <typename EdgeScoringFunction>
	void mergeRegions(
			EdgeIdType e,
			EdgeScoringFunction& edgeScoringFunction) {

		NodeIdType a = _regionGraph.edge(e).u;
		NodeIdType b = _regionGraph.edge(e).v;

		// assign new node a = a + b
		edgeScoringFunction.notifyNodeMerge(b, a);

		// set path
		_rootPaths[b] = a;

		// mark all incident edges of a as stale...
		for (EdgeIdType neighborEdge : _regionGraph.incEdges(a))
			_stale[neighborEdge] = true;

		// ...and update incident edges of b
		std::vector<EdgeIdType> neighborEdges = _regionGraph.incEdges(b);
		for (EdgeIdType neighborEdge : neighborEdges) {

			if (neighborEdge == e)
				continue;

			NodeIdType neighbor = _regionGraph.getOpposite(b, neighborEdge);

			// There are two kinds of neighbors of b:
			//
			//   1. exclusive to b
			//   2. shared by a and b

			EdgeIdType aNeighborEdge = _regionGraph.findEdge(a, neighbor);

			if (aNeighborEdge == RegionGraphType::NoEdge) {

				// We encountered an exclusive neighbor of b.

				_regionGraph.moveEdge(neighborEdge, a, neighbor);
				assert(_regionGraph.findEdge(a, neighbor) == neighborEdge);
				_stale[neighborEdge] = true;

			} else {

				// We encountered a shared neighbor. We have to:
				//
				// * merge the more expensive edge one into the cheaper one
				// * mark the cheaper one as stale (if it isn't already)
				// * delete the more expensive one
				//
				// This ensures that the stale edge bubbles up early enough 
				// to consider it's real score (which is assumed to be 
				// larger than the minium of the two original scores).

				if (_edgeScores[neighborEdge] > _edgeScores[aNeighborEdge]) {

					// We got lucky, we can reuse the edge that is attached to a 
					// already

					edgeScoringFunction.notifyEdgeMerge(neighborEdge, aNeighborEdge);
					_deleted[neighborEdge] = true;

				} else {

					// Bummer. The new edge should be the one pointing from 
					// a to neighbor.

					edgeScoringFunction.notifyEdgeMerge(aNeighborEdge, neighborEdge);

					_regionGraph.removeEdge(aNeighborEdge);
					_regionGraph.moveEdge(neighborEdge, a, neighbor);
					assert(_regionGraph.findEdge(a, neighbor) == neighborEdge);

					_stale[neighborEdge] = true;
					_deleted[aNeighborEdge] = true;
				}
			}
		}
	}

	/**
	 * Score edge e.
	 */
	template <typename EdgeScoringFunction>
	ScoreType scoreEdge(EdgeIdType e, EdgeScoringFunction& edgeScoringFunction) {

		ScoreType score = edgeScoringFunction(e);

		_edgeScores[e] = score;
		_edgeQueue.push(e);

		return score;
	}

	inline bool isRoot(NodeIdType id) {

		// if there is no root path, it is a root
		return (_rootPaths.count(id) == 0);
	}

	/**
	 * Get the root node of a merge-tree.
	 */
	NodeIdType getRoot(NodeIdType id) {

		// early way out
		if (isRoot(id))
			return id;

		// walk up to root

		NodeIdType root = _rootPaths.at(id);
		while (!isRoot(root))
			root = _rootPaths.at(root);

		// not compressed, yet
		if (_rootPaths.at(id) != root)
			while (id != root) {

				NodeIdType next = _rootPaths.at(id);
				_rootPaths[id] = root;
				id = next;
			}

		return root;
	}

	RegionGraphType& _regionGraph;

	// the score of each edge
	typename RegionGraphType::template EdgeMap<ScoreType> _edgeScores;

	typename RegionGraphType::template EdgeMap<bool> _stale;
	typename RegionGraphType::template EdgeMap<bool> _deleted;

	// sorted list of edges indices, cheapest edge first
	std::priority_queue<EdgeIdType, std::vector<EdgeIdType>, EdgeCompare> _edgeQueue;

	// paths from nodes to the roots of the merge-tree they are part of
	//
	// root nodes are not in the map
	//
	// paths will be compressed when read
	std::map<NodeIdType, NodeIdType> _rootPaths;

	// current state of merging
	ScoreType _mergedUntil;
};

#endif // ITERATIVE_REGION_MERGING_H__

