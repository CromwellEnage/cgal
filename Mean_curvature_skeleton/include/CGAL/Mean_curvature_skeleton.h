#ifndef MEAN_CURVATURE_SKELETON_H
#define MEAN_CURVATURE_SKELETON_H

#include <CGAL/trace.h>
#include <CGAL/Timer.h>
#include <CGAL/boost/graph/graph_traits_Polyhedron_3.h>
#include <CGAL/boost/graph/properties_Polyhedron_3.h>
#include <CGAL/boost/graph/halfedge_graph_traits_Polyhedron_3.h>

#include <CGAL/internal/Mean_curvature_skeleton/Weights.h>

namespace CGAL {

template <class Polyhedron, class SparseLinearAlgebraTraits_d,
          class PolyhedronVertexIndexMap, class PolyhedronEdgeIndexMap>
class Mean_curvature_skeleton
{
// Public types
public:

  // Geometric types
  typedef typename Polyhedron::Traits         Kernel;
  typedef typename Kernel::Vector_3           Vector;
  typedef typename Kernel::Point_3            Point;

  // Repeat Polyhedron types
  typedef typename boost::graph_traits<Polyhedron>::vertex_descriptor	         vertex_descriptor;
  typedef typename boost::graph_traits<Polyhedron>::vertex_iterator            vertex_iterator;
  typedef typename boost::graph_traits<Polyhedron>::edge_descriptor            edge_descriptor;
  typedef typename boost::graph_traits<Polyhedron>::edge_iterator              edge_iterator;
  typedef typename boost::graph_traits<Polyhedron>::in_edge_iterator           in_edge_iterator;
  typedef typename internal::Cotangent_weight<Polyhedron>                      Weight_calculator;

  // Data members.
private:

  Polyhedron* polyhedron;
  PolyhedronVertexIndexMap vertex_id_pmap;
  PolyhedronEdgeIndexMap edge_id_pmap;

  Weight_calculator weight_calculator;
  std::vector<double> edge_weight;
  SparseLinearAlgebraTraits_d m_solver;

  double omega_L;
  double omega_H;

  // Public methods
public:

  // The constructor gets the polyhedron that we will model
  Mean_curvature_skeleton(Polyhedron* P, PolyhedronVertexIndexMap Vertex_index_map,
                          PolyhedronEdgeIndexMap Edge_index_map, double omega_L, double omega_H,
                          Weight_calculator weight_calculator = Weight_calculator()
                          )
    :polyhedron(P), vertex_id_pmap(Vertex_index_map), edge_id_pmap(Edge_index_map),
      omega_L(omega_L), omega_H(omega_H), weight_calculator(weight_calculator)
  {
    // initialize index maps
    vertex_iterator vb, ve;
    int idx = 0;
    for (boost::tie(vb, ve) = boost::vertices(*polyhedron); vb != ve; ++vb)
    {
      boost::put(vertex_id_pmap, *vb, idx++);
    }

    edge_iterator eb, ee;
    idx = 0;
    for (boost::tie(eb, ee) = boost::edges(*polyhedron); eb != ee; ++eb)
    {
      boost::put(edge_id_pmap, *eb, idx++);
    }
  }

  // Release resources
  ~Mean_curvature_skeleton(void)
  {
  }

  // compute cotangent weights of all edges
  void compute_edge_weight()
  {
    edge_weight.reserve(boost::num_edges(*polyhedron));
    edge_iterator eb, ee;
    for(boost::tie(eb, ee) = boost::edges(*polyhedron); eb != ee; ++eb)
    {
      edge_weight.push_back(this->weight_calculator(*eb, *polyhedron));
    }
  }

  void assemble_LHS(typename SparseLinearAlgebraTraits_d::Matrix& A)
  {
    int nver = boost::num_vertices(*polyhedron);

    // initialize the Laplacian matrix
    for (int i = 0; i < nver; i++)
    {
      A.set_coef(i, i, 0.0, true);
      A.set_coef(i + nver, i, omega_H, true);
    }

    vertex_iterator vb, ve;
    for (boost::tie(vb, ve) = boost::vertices(*polyhedron); vb != ve; vb++)
    {
      int i = boost::get(vertex_id_pmap, *vb);
      double diagonal = 0;
      in_edge_iterator e, e_end;
      for (boost::tie(e, e_end) = boost::in_edges(*vb, *polyhedron); e != e_end; e++)
      {
        vertex_descriptor vj = boost::source(*e, *polyhedron);
        double wij = edge_weight[boost::get(edge_id_pmap, *e)] * 2.0;
        int j = boost::get(vertex_id_pmap, vj);
        A.set_coef(i, j, wij * omega_L, true);
        diagonal += -wij;
      }
      A.set_coef(i, i, diagonal);
    }
  }

  void assemble_RHS(typename SparseLinearAlgebraTraits_d::Vector& Bx,
                    typename SparseLinearAlgebraTraits_d::Vector& By,
                    typename SparseLinearAlgebraTraits_d::Vector& Bz)
  {
    // assemble right columns of linear system
    int nver = boost::num_vertices(*polyhedron);;
    vertex_iterator vb, ve;
    for (int i = 0; i < nver; i++)
    {
      Bx[i] = 0;
      By[i] = 0;
      Bz[i] = 0;
    }
    for (boost::tie(vb, ve) = boost::vertices(*polyhedron); vb != ve; vb++)
    {
      vertex_descriptor vi = *vb;
      int i = boost::get(vertex_id_pmap, vi);
      Bx[i + nver] = vi->point().x() * omega_H;
      By[i + nver] = vi->point().y() * omega_H;
      Bz[i + nver] = vi->point().z() * omega_H;
    }
  }

  void contract_geometry()
  {
    compute_edge_weight();

    // Assemble linear system At * A * X = At * B
    int nver = boost::num_vertices(*polyhedron);
    typename SparseLinearAlgebraTraits_d::Matrix A(nver * 2, nver);
    assemble_LHS(A);

    typename SparseLinearAlgebraTraits_d::Vector X(nver), Bx(nver * 2);
    typename SparseLinearAlgebraTraits_d::Vector Y(nver), By(nver * 2);
    typename SparseLinearAlgebraTraits_d::Vector Z(nver), Bz(nver * 2);
    assemble_RHS(Bx, By, Bz);

    // solve "At * A * X = At * B".
    double D;
    m_solver.pre_factor_non_symmetric(A, D);
    m_solver.linear_solver_non_symmetric(A, Bx, X);
    m_solver.linear_solver_non_symmetric(A, By, Y);
    m_solver.linear_solver_non_symmetric(A, Bz, Z);

    // copy to mesh
    vertex_iterator vb, ve;
    for (boost::tie(vb, ve) = boost::vertices(*polyhedron); vb != ve; vb++)
    {
      vertex_descriptor vi = *vb;
      int i = boost::get(vertex_id_pmap, vi);
      Point p(X[i], Y[i], Z[i]);
      vi->point() = p;
    }
  }
};

} //namespace CGAL

#endif // MEAN_CURVATURE_SKELETON_H
