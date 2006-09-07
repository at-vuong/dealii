/* $Id: step-4.cc,v 1.34 2006/02/06 21:33:10 wolf Exp $ */
/* Author: Wolfgang Bangerth, Texas A&M University, 2006 */

/*    $Id: step-4.cc,v 1.34 2006/02/06 21:33:10 wolf Exp $       */
/*    Version: $Name:  $                                          */
/*                                                                */
/*    Copyright (C) 2006 by the deal.II authors */
/*                                                                */
/*    This file is subject to QPL and may not be  distributed     */
/*    without copyright and license information. Please refer     */
/*    to the file deal.II/doc/license.html for the  text  and     */
/*    further information on this license.                        */


				 // @sect3{Include files}

				 // We start with the usual assortment
				 // of include files that we've seen
				 // in so many of the previous tests:
#include <base/quadrature_lib.h>
#include <base/function.h>
#include <base/logstream.h>

#include <lac/vector.h>
#include <lac/full_matrix.h>
#include <lac/sparse_matrix.h>
#include <lac/solver_cg.h>
#include <lac/precondition.h>

#include <grid/tria.h>
#include <grid/grid_generator.h>
#include <grid/tria_accessor.h>
#include <grid/tria_iterator.h>

#include <dofs/dof_handler.h>
#include <dofs/dof_accessor.h>
#include <dofs/dof_tools.h>
#include <dofs/dof_constraints.h>

#include <fe/fe_q.h>
#include <fe/fe_values.h>

#include <numerics/data_out.h>

#include <fstream>
#include <iostream>
#include <sstream>

				 // Here are the only three include
				 // files of some new interest: The
				 // first one is already used, for
				 // example, for the
				 // VectorTools::interpolate_boundary_values
				 // and
				 // VectorTools::apply_boundary_values
				 // functions. However, we here use
				 // another function in that class,
				 // VectorTools::project to compute
				 // our initial values as the $L^2$
				 // projection of the continuous
				 // initial values. Furthermore, we
				 // use
				 // VectorTools::create_right_hand_side
				 // to generate the integrals
				 // $(f^n,\phi^n_i)$. These were
				 // previously always generated by
				 // hand in
				 // <code>assemble_system</code> or
				 // similar functions in application
				 // code. However, we're too lazy to
				 // do that here, so simply use a
				 // library function:
#include <numerics/vectors.h>

				 // In a very similar vein, we are
				 // also too lazy to write the code to
				 // assemble mass and Laplace
				 // matrices, although it would have
				 // only taken copying the relevant
				 // code from any number of previous
				 // tutorial programs. Rather, we want
				 // to focus on the things that are
				 // truly new to this program and
				 // therefore use the
				 // MatrixTools::create_mass_matrix
				 // and
				 // MatrixTools::create_laplace_matrix
				 // functions. They are declared here:
#include <numerics/matrices.h>

				 // Finally, here is an include file
				 // that contains all sorts of tool
				 // functions that one sometimes
				 // needs. In particular, we need the
				 // Utilities::int_to_string class
				 // that, given an integer argument,
				 // returns a string representation of
				 // it. It is particularly useful
				 // since it allows for a second
				 // parameter indicating the number of
				 // digits to which we want the result
				 // padded with leading zeros. We will
				 // use this to write output files
				 // that have the form
				 // <code>solution-XXX.gnuplot</code>
				 // where <code>XXX</code> denotes the
				 // number of the time step and always
				 // consists of three digits even if
				 // we are still in the single or
				 // double digit time steps.
#include <base/utilities.h>


				 // @sect3{The <code>WaveEquation</code> class}

				 // Next comes the declaration of the
				 // main class. It's public interface
				 // of functions is like in most of
				 // the other tutorial programs. Worth
				 // mentioning is that we now have to
				 // store three matrices instead of
				 // one: the mass matrix $M$, the
				 // Laplace matrix $A$, and the system
				 // matrix $M+k^2\theta^2A$ used when
				 // solving for $U^n$. Likewise, we
				 // need solution vectors for
				 // $U^n,V^n$ as well as for the
				 // corresponding vectors at the
				 // previous time step,
				 // $U^{n-1},V^{n-1}$. The
				 // <code>system_rhs</code> will be
				 // used for whatever right hand side
				 // vector we have when solving one of
				 // the two linear systems we have to
				 // solve in each time step. These
				 // will be solved in the two
				 // functions <code>solve_u</code> and
				 // <code>solve_v</code>.
				 //
				 // Finally, the variable
				 // <code>theta</code> is used to
				 // indicate the parameter $\theta$
				 // that is used to define which time
				 // stepping scheme to use. The rest
				 // is self-explanatory.
template <int dim>
class WaveEquation 
{
  public:
    WaveEquation ();
    void run ();
    
  private:
    void setup_system ();
    void solve_u ();
    void solve_v ();
    void output_results () const;

    Triangulation<dim>   triangulation;
    FE_Q<dim>            fe;
    DoFHandler<dim>      dof_handler;

    ConstraintMatrix constraints;
    
    SparsityPattern      sparsity_pattern;
    SparseMatrix<double> system_matrix;
    SparseMatrix<double> mass_matrix;
    SparseMatrix<double> laplace_matrix;

    Vector<double>       solution_u, solution_v;
    Vector<double>       old_solution_u, old_solution_v;
    Vector<double>       system_rhs;

    double time, time_step;
    unsigned int timestep_number;
    const double theta;
};



				 // @sect3{Equation data}

				 // Before we go on filling in the
				 // details of the main class, let us
				 // define the equation data
				 // corresponding to the problem,
				 // i.e. initial and boundary values
				 // as well as a right hand side
				 // class. We do so using classes
				 // derived from the Function class
				 // template that has been used many
				 // times before, so the following
				 // should not be a surprise.
				 //
				 // Let's start with initial values
				 // and choose zero for both the value
				 // $u$ as well as its time
				 // derivative, the velocity $v$:
template <int dim>
class InitialValuesU : public Function<dim> 
{
  public:
    InitialValuesU () : Function<dim>() {};
    
    virtual double value (const Point<dim>   &p,
			  const unsigned int  component = 0) const;
};


template <int dim>
class InitialValuesV : public Function<dim> 
{
  public:
    InitialValuesV () : Function<dim>() {};
    
    virtual double value (const Point<dim>   &p,
			  const unsigned int  component = 0) const;
};



template <int dim>
double InitialValuesU<dim>::value (const Point<dim>  &/*p*/,
				   const unsigned int component) const 
{
  Assert (component == 0, ExcInternalError());
  return 0;
}



template <int dim>
double InitialValuesV<dim>::value (const Point<dim>  &/*p*/,
				   const unsigned int component) const 
{
  Assert (component == 0, ExcInternalError());
  return 0;
}



				 // Secondly, we have the right hand
				 // side forcing term. Boring as we
				 // are, we choose zero here as well:
template <int dim>
class RightHandSide : public Function<dim> 
{
  public:
    RightHandSide () : Function<dim>() {};
    
    virtual double value (const Point<dim>   &p,
			  const unsigned int  component = 0) const;
};



template <int dim>
double RightHandSide<dim>::value (const Point<dim>  &/*p*/,
				  const unsigned int component) const 
{
  Assert (component == 0, ExcInternalError());
  return 0;
}



				 // Finally, we have boundary
				 // values. They are as described in
				 // the introduction:
template <int dim>
class BoundaryValues : public Function<dim> 
{
  public:
    BoundaryValues () : Function<dim>() {};
    
    virtual double value (const Point<dim>   &p,
			  const unsigned int  component = 0) const;
};




template <int dim>
double BoundaryValues<dim>::value (const Point<dim> &p,
				   const unsigned int component) const 
{
  Assert (component == 0, ExcInternalError());

  if ((this->get_time() <= 1) &&
      (p[0] < 1) &&
      (p[1] < 1./3) &&
      (p[1] > -1./3))
    return std::sin (this->get_time() * 2 * deal_II_numbers::PI);
  else
    return 0;
}




				 // @sect3{Implementation of the <code>WaveEquation</code> class}


template <int dim>
WaveEquation<dim>::WaveEquation () :
                fe (1),
		dof_handler (triangulation),
		time_step (1./64),
		theta (0.5)
{}



template <int dim>
void WaveEquation<dim>::setup_system ()
{
  GridGenerator::hyper_cube (triangulation, -1, 1);
  triangulation.refine_global (7);
  
  std::cout << "Number of active cells: "
	    << triangulation.n_active_cells()
	    << std::endl
	    << "Total number of cells: "
	    << triangulation.n_cells()
	    << std::endl
  	    << std::endl;

  dof_handler.distribute_dofs (fe);

  std::cout << "   Number of degrees of freedom: "
	    << dof_handler.n_dofs()
	    << std::endl;

  sparsity_pattern.reinit (dof_handler.n_dofs(),
			   dof_handler.n_dofs(),
			   dof_handler.max_couplings_between_dofs());
  DoFTools::make_sparsity_pattern (dof_handler, sparsity_pattern);
  sparsity_pattern.compress();

  system_matrix.reinit (sparsity_pattern);
  mass_matrix.reinit (sparsity_pattern);
  laplace_matrix.reinit (sparsity_pattern);

  MatrixCreator::create_mass_matrix (dof_handler, QGauss<dim>(3),
				     mass_matrix);
  MatrixCreator::create_laplace_matrix (dof_handler, QGauss<dim>(3),
					laplace_matrix);
  
  system_matrix.copy_from (mass_matrix);
  system_matrix.add (theta * theta * time_step * time_step, laplace_matrix);
  
  solution_u.reinit (dof_handler.n_dofs());
  solution_v.reinit (dof_handler.n_dofs());
  old_solution_u.reinit (dof_handler.n_dofs());
  old_solution_v.reinit (dof_handler.n_dofs());
  system_rhs.reinit (dof_handler.n_dofs());

  constraints.close ();
}



template <int dim>
void WaveEquation<dim>::solve_u () 
{
  SolverControl           solver_control (1000, 1e-8*system_rhs.l2_norm());
  SolverCG<>              cg (solver_control);
  cg.solve (system_matrix, solution_u, system_rhs,
	    PreconditionIdentity());

  std::cout << "   u-equation: " << solver_control.last_step()
	    << " CG iterations."
	    << std::endl;
}


template <int dim>
void WaveEquation<dim>::solve_v () 
{
  SolverControl           solver_control (1000, 1e-8*system_rhs.l2_norm());
  SolverCG<>              cg (solver_control);
  cg.solve (mass_matrix, solution_v, system_rhs,
	    PreconditionIdentity());

  std::cout << "   v-equation: " << solver_control.last_step()
	    << " CG iterations."
	    << std::endl;
}



template <int dim>
void WaveEquation<dim>::output_results () const
{
  DataOut<dim> data_out;

  data_out.attach_dof_handler (dof_handler);
  data_out.add_data_vector (solution_u, "U");
  data_out.add_data_vector (solution_v, "V");

  data_out.build_patches ();

  std::ostringstream filename;
  filename << "solution-"
	   << Utilities::int_to_string (timestep_number, 3)
	   << ".gnuplot";
  std::ofstream output (filename.str().c_str());
  data_out.write_gnuplot (output);
}




template <int dim>
void WaveEquation<dim>::run () 
{
  setup_system();

  VectorTools::project (dof_handler, constraints, QGauss<dim>(3),
			InitialValuesU<dim>(),
			old_solution_u);
  VectorTools::project (dof_handler, constraints, QGauss<dim>(3),
			InitialValuesV<dim>(),
			old_solution_v);
  
  for (timestep_number=1, time=time_step; time<=5; time+=time_step, ++timestep_number)
    {
      std::cout << "Time step " << timestep_number
		<< " at t=" << time
		<< std::endl;
      
      Vector<double> tmp (solution_u.size());

      mass_matrix.vmult (system_rhs, old_solution_u);

      mass_matrix.vmult (tmp, old_solution_v);
      system_rhs.add (time_step, tmp);

      laplace_matrix.vmult (tmp, old_solution_u);
      system_rhs.add (-theta * (1-theta) * time_step * time_step, tmp);

      RightHandSide<dim> rhs_function;
      rhs_function.set_time (time);
      VectorTools::create_right_hand_side (dof_handler, QGauss<dim>(2),
					   rhs_function, tmp);
      system_rhs.add (theta * theta * time_step * time_step, tmp);

      rhs_function.set_time (time-time_step);
      VectorTools::create_right_hand_side (dof_handler, QGauss<dim>(2),
					   rhs_function, tmp);
      system_rhs.add (theta * (1-theta) * time_step * time_step, tmp);


      BoundaryValues<dim> boundary_values_function;
      boundary_values_function.set_time (time);
      
      std::map<unsigned int,double> boundary_values;
      VectorTools::interpolate_boundary_values (dof_handler,
						0,
						boundary_values_function,
						boundary_values);
      MatrixTools::apply_boundary_values (boundary_values,
					  system_matrix,
					  solution_u,
					  system_rhs);
      solve_u ();


      laplace_matrix.vmult (system_rhs, solution_u);
      system_rhs *= -theta * time_step;

      mass_matrix.vmult (tmp, old_solution_v);
      system_rhs += tmp;

      laplace_matrix.vmult (tmp, old_solution_u);
      system_rhs.add (-time_step * (1-theta), tmp);

      rhs_function.set_time (time);
      VectorTools::create_right_hand_side (dof_handler, QGauss<dim>(2),
					   rhs_function, tmp);
      system_rhs.add (theta * time_step, tmp);

      rhs_function.set_time (time-time_step);
      VectorTools::create_right_hand_side (dof_handler, QGauss<dim>(2),
					   rhs_function, tmp);
      system_rhs.add ((1-theta) * time_step, tmp);

      solve_v ();

      output_results ();

      old_solution_u = solution_u;
      old_solution_v = solution_v;
    }
}



int main () 
{
  deallog.depth_console (0);
  {
    WaveEquation<2> wave_equation_solver;
    wave_equation_solver.run ();
  }
  
  return 0;
}
