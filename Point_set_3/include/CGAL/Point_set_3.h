// Copyright (c) 2016  GeometryFactory Sarl (France).
// All rights reserved.
//
// This file is part of CGAL (www.cgal.org).
// You can redistribute it and/or modify it under the terms of the GNU
// General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.
//
// Licensees holding a valid commercial license may use this file in
// accordance with the commercial license agreement provided with the software.
//
// This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
// WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
//
// $URL$
// $Id$
//
//
// Author(s)     : Simon Giraudot

#ifndef CGAL_POINT_SET_3_H
#define CGAL_POINT_SET_3_H

#include <stack>

#include <CGAL/Surface_mesh/Properties.h>

namespace CGAL {



/*!

  \ingroup PkgPointSet3

  \brief A collection of 3D points.

  This class provides the user with a flexible way to store and access
  a point set:

  - it can embed a random number of additional attributes such as
    normal vectors, colors, indices, etc.;

  - all functions of the package \ref PkgPointSetProcessing are
    provided with an overload that take a `Point_set_3` object as an
    argument.

  \tparam Gt Geometric traits class.

 */

template <class Gt>
class Point_set_3
{
public:

  typedef Point_set_3<Gt> Point_set;
  typedef typename Gt::FT FT;
  typedef typename Gt::Point_3 Point;
  typedef typename Gt::Vector_3 Vector;
  typedef typename Gt::Iso_cuboid_3 Iso_cuboid;
  typedef typename Gt::Sphere_3 Sphere;
  

  typedef typename std::size_t Item;

  typedef typename Properties::Property_container<Item> Base;

  template <class Type>
  struct Property_map
  {
    typedef typename Properties::Property_map<Item, Type> type;
  };
  
  typedef typename Property_map<std::size_t>::type Index_pmap;
  typedef typename Property_map<Point>::type Point_pmap;
  typedef typename Property_map<Vector>::type Vector_pmap;

  typedef typename Index_pmap::iterator iterator;
  typedef typename Index_pmap::const_iterator const_iterator;

  template <typename Property>
  class Property_back_inserter {
  public:
    typedef std::output_iterator_tag iterator_category;
    typedef typename Property::value_type value_type;
    typedef std::ptrdiff_t           difference_type;
    typedef void                     pointer;
    typedef void                     reference;

  private:

    Point_set* ps;
    Property* prop;
    std::size_t ind;
  
  public:
    
    Property_back_inserter(Point_set* ps, Property* prop, std::size_t ind=0)
      : ps(ps), prop (prop), ind(ind) {}
    Property_back_inserter& operator++() { return *this; }
    Property_back_inserter& operator++(int) { return *this; }
    Property_back_inserter& operator*() { return *this; }
    Property_back_inserter& operator= (const value_type& p)
    {
      if(ps->size() <= (typename Point_set::Item(ind)))
        ps->add_item();
      put(*prop, Point_set::Item(ind), p);
      ++ ind;
      return *this;
    }
                                  
  };

  template <typename Property>
  class Push_pmap
  {
  public:
    typedef std::size_t key_type;
    typedef typename Property::value_type value_type;
    typedef value_type& reference;
    typedef boost::lvalue_property_map_tag category;
    
    Point_set* ps;
    Property* prop;
    mutable std::size_t ind;

    Push_pmap(Point_set* ps = NULL,
              Property* prop = NULL,
              std::size_t ind=0)
      : ps(ps), prop(prop), ind(ind) {}

    friend void put(const Push_pmap& pm, std::size_t& i, const typename Property::value_type& t)
    {
      if(pm.ps->size() <= (pm.ind))
        pm.ps->add_item();
      put(*(pm.prop), Point_set::Item(pm.ind), t);
      i = pm.ind;
      ++pm.ind;
    }

    friend const reference get (const Push_pmap& pm, const std::size_t& i)
    {
      return ((*(pm.prop))[i]);
    }
  };


  typedef Property_back_inserter<Index_pmap> Index_back_inserter;
  typedef Property_back_inserter<Point_pmap> Point_back_inserter;
  typedef Push_pmap<Point_pmap> Point_push_pmap;
  typedef Push_pmap<Vector_pmap> Vector_push_pmap;


protected:

  Base m_base;
  Index_pmap m_indices;
  Point_pmap m_points;
  Vector_pmap m_normals;
  std::size_t m_nb_removed;

  // Assignment operator not implemented and declared private to make
  // sure nobody uses the default one without knowing it
  Point_set_3& operator= (const Point_set_3&)
  {
    return *this;
  }

  // Copy constructor not implemented and declared private to make
  // sure nobody uses the default one without knowing it
  Point_set_3 (const Point_set_3& p)
  {
  }

  
public:

  Point_set_3 () : m_base()
  {
    m_indices = m_base.template add<std::size_t> ("index").first;
    m_points = m_base.template add<Point> ("point").first;
    m_nb_removed = 0;
  }

  void push_back (const Point& p)
  {
    add_item();
    m_indices[size()-1] = size()-1;
    m_points[size()-1] = p;
  }
  void push_back (const Point& p, const Vector& n)
  {
    push_back (p);
    assert (has_normals());
    m_normals[size()-1] = n;
  }

  iterator begin() { return m_indices.begin(); }
  iterator end() { return m_indices.end() - m_nb_removed; }
  const_iterator begin() const { return m_indices.begin(); }
  const_iterator end() const { return m_indices.end() - m_nb_removed; }
  bool empty() const { return (m_base.size() == m_nb_removed); }
  std::size_t size () const { return m_base.size() - m_nb_removed; }
  void reserve (std::size_t s) { m_base.reserve (s); }
  void resize (std::size_t s)
  {
    m_base.resize (s);
    if (s > size ())
      m_nb_removed += s;
    else if (m_base.size() - s > m_nb_removed)
      m_nb_removed = 0;
    else
      m_nb_removed -= s;
  }

  iterator removed_begin () { return m_indices.end() - m_nb_removed; }
  iterator removed_end () { return m_indices.end(); }
  const_iterator removed_begin () const { return m_indices.end() - m_nb_removed; }
  const_iterator removed_end () const { return m_indices.end(); }
  std::size_t removed_size () const { return m_nb_removed; }
  bool has_garbage () const { return (m_nb_removed != 0); }  

  void collect_garbage ()
  {
    // Indices indicate where to get the properties
    std::vector<std::size_t> indices (m_base.size());
    for (std::size_t i = 0; i < m_base.size(); ++ i)
      indices[m_indices[i]] = i;

    // Indices now indicate where to put the properties
    for (std::size_t i = 0; i < m_base.size(); ++ i)
      m_indices[i] = indices[i];

    // for (std::size_t i = 0; i < 10; ++ i)
    //   std::cerr << m_indices[i] << " ";
    // std::cerr << std::endl;

    // Sorting based on the indices reorders the point set correctly
    quick_sort_on_indices ((std::ptrdiff_t)0, (std::ptrdiff_t)(m_base.size() - 1));

    // for (std::size_t i = 0; i < 10; ++ i)
    //   std::cerr << m_indices[i] << " ";
    // std::cerr << std::endl;

    m_base.resize (size ());
    m_base.shrink_to_fit ();
    m_nb_removed = 0;
  }

  void clear()
  {
    m_base.clear();
    m_indices = m_base.template add<std::size_t> ("index").first;
    m_points = m_base.template add<Point> ("point").first;
    m_nb_removed = 0;
  }

  Point& operator[] (Item index) { return m_points[m_indices[index]]; }
  const Point& operator[] (Item index) const { return m_points[m_indices[index]]; }
  Point& point (Item index) { return m_points[m_indices[index]]; }
  const Point& point (Item index) const { return m_points[m_indices[index]]; }
  Point& point (iterator it) { return m_points[*it]; }
  const Point& point (const_iterator it) const { return m_points[*it]; }

  void add_item ()
  {
    m_base.push_back();
  }

  void remove_from (iterator it)
  {
    m_nb_removed = static_cast<std::size_t>(std::distance (it, removed_end()));
  }
  
  void remove (iterator it)
  {
    iterator first = removed_begin();
    std::swap (*it, *(removed_begin() - 1));
    -- m_nb_removed;
  }

  Index_back_inserter index_back_inserter ()
  {
    return Index_back_inserter (this, &m_indices, size());
  }
  Point_back_inserter point_back_inserter ()
  {
    return Point_back_inserter (this, &m_points, size());
  }
  
  Point_push_pmap point_push_pmap ()
  {
    return Point_push_pmap (this, &m_points, size());
  }
  // Point_pmap point_pmap ()
  // {
  //   return Point_pmap (this, &m_points);
  // }
  // Const_point_pmap point_pmap () const
  // {
  //   return Const_point_pmap (this, &m_points);
  // }

  Point_pmap point_pmap()
  {
    return m_points;
  }

  const Point_pmap point_pmap() const
  {
    return m_points;
  }

  Vector_pmap normal_pmap ()
  {
    return m_normals;
  }
  Vector_push_pmap normal_push_pmap ()
  {
    return Vector_push_pmap (this, &m_normals, size());
  }
  const Vector_pmap normal_pmap () const
  {
    return m_normals;
  }

    
  bool has_normals() const
  {
    std::pair<Vector_pmap, bool> pm = m_base.template get<Vector> ("normal");
    return pm.second;
  }
  bool add_normal_property()
  {
    bool out = false;
    boost::tie (m_normals, out) = m_base.template add<Vector> ("normal");
    return out;
  }
  void remove_normal_property()
  {
    m_base.remove (m_normals);
  }
  Vector& normal (Item index) { return m_normals[m_indices[index]]; }
  const Vector& normal (Item index) const { return m_normals[m_indices[index]]; }
  Vector& normal (iterator it) { return m_normals[*it]; }
  const Vector& normal (const_iterator it) const { return m_normals[*it]; }

  template <typename T>
  bool has_property (const std::string& name) const
  {
    std::pair<typename Properties::template Property_map<Item, T>, bool>
      pm = m_base.template get<T> (name);
    return pm.second;
  }
  
  template <class T>
  std::pair<typename Property_map<T>::type, bool>
  add_property (const std::string& name, const T t=T())
  {
    return m_base.add<T> (name, t);
  }
  
  template <class T> 
  std::pair<Properties::Property_map<Item, T>,bool>
  get_property (const std::string& name) const
  {
    return m_base.template get<T>(name);
  }
  
  template <class T> 
  void remove_property (Properties::Property_map<Item, T>& prop)
  {
    m_base.remove (prop);
  }

  std::string info() const
  {
    std::ostringstream oss;
    oss << "Point_set_3 with " << size() << " point(s) ("
        << removed_size() << " removed point(s) waiting to be deleted)" << std::endl;
    std::vector<std::string> prop = m_base.properties();
    for (std::size_t i = 0; i < prop.size(); ++ i)
      oss << " * \"" << prop[i] << "\" property of type "
          << boost::core::demangle(m_base.get_type(prop[i]).name()) << std::endl;

    return oss.str();
  }

private:

  void quick_sort_on_indices (std::ptrdiff_t begin, std::ptrdiff_t end)
  {
    std::stack<std::pair<std::ptrdiff_t, std::ptrdiff_t> >
      todo;
    todo.push (std::make_pair (begin, end));
    
    while (!(todo.empty()))
      {
        std::pair<std::ptrdiff_t, std::ptrdiff_t>
          current = todo.top();
        todo.pop();
        
        if (current.first < current.second)
          {
            std::ptrdiff_t p = current.first + (rand() % (current.second - current.first));
            p = quick_sort_partition (current.first, current.second, p);
            todo.push (std::make_pair (current.first, p-1));
            todo.push (std::make_pair (p+1, current.second));
          }
      }
  }

  std::ptrdiff_t quick_sort_partition (std::ptrdiff_t begin, std::ptrdiff_t end, std::ptrdiff_t p)
  {
    m_base.swap (p, end);
    std::ptrdiff_t j = begin;
    for (std::ptrdiff_t i = begin; i < end; ++ i)
      if (m_indices[i] <= m_indices[end])
        {
          m_base.swap (i, j);
          j ++;
        }
    m_base.swap (end, j);
    return j;
  }
  

  
}; // end of class Point_set_3

} // namespace CGAL


#endif // CGAL_POINT_SET_3_H
