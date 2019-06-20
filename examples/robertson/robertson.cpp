/*
 * Solves Robertson Problem as Semi-Explicit Differential Algebraic Equations
 * (see https://www.mathworks.com/help/matlab/ref/ode15s.html):
 *
 * x1' = -0.04*x1 + 1e4*x2*x3
 * x2' =  0.04*x1 - 1e4*x2*x3 - 3e7*x2^2
 *  0  =  x1 + x2 + x3 - 1
 *
 * Initial conditions are: x1 = 1, x2 = 0, x3 = 0.
 *
 * The 3rd equation in the system is basically a conservation law. It will be
 * tested that x1 + x2 + x3 = 1 exactly every time step.
 *
 * From MATLAB ode15s description:
 *
 *   This problem is used as an example in the prolog to LSODI [1]. Though
 *   consistent initial conditions are obvious, the guess x3 = 1e-3 is used
 *   to test initialization. A logarithmic scale is appropriate for plotting
 *   the solution on the long time interval. x2 is small and its major change
 *   takes place in a relatively short time.
 *
 *   [1] A.C. Hindmarsh, LSODE and LSODI, two new initial value ordinary
 *       differential equation solvers, SIGNUM Newsletter, 15 (1980), pp. 10-11.
 *
 * Keywords: Robertson problem, stiff DAE system, comparison with MATLAB ode15s.
 */

#include <iostream>
#include <cmath>

#include "../../src/solver.h"  // the main header of dae-cpp library solver

using namespace daecpp;

// python3 + numpy + matplotlib should be installed in order to enable plotting
// #define PLOTTING

#ifdef PLOTTING
#include "../../src/external/matplotlib-cpp/matplotlibcpp.h"
namespace plt = matplotlibcpp;
#endif

/*
 * Singular mass matrix in 3-array sparse format
 * =============================================================================
 * The matrix has the following form:
 *     |1 0 0|
 * M = |0 1 0|
 *     |0 0 0|
 *
 * For more information about the sparse format see
 * https://software.intel.com/en-us/mkl-developer-reference-c-sparse-blas-csr-matrix-storage-format
 */
class MyMassMatrix : public MassMatrix
{
public:
    void operator()(daecpp::sparse_matrix_holder &M)
    {
        M.A.resize(3);   // Matrix size
        M.ja.resize(3);  // Matrix size
        M.ia.resize(4);  // Matrix size + 1

        // Non-zero and/or diagonal elements
        M.A[0] = 1;
        M.A[1] = 1;
        M.A[2] = 0;

        // Column index of each element given above
        M.ja[0] = 0;
        M.ja[1] = 1;
        M.ja[2] = 2;

        // Index of the first element for each row
        M.ia[0] = 0;
        M.ia[1] = 1;
        M.ia[2] = 2;
        M.ia[3] = 3;
    }
};

/*
 * RHS of the problem
 * =============================================================================
 */
class MyRHS : public RHS
{
public:
    /*
     * Receives current solution vector x and the current time t. Defines the
     * RHS f for each element in x.
     */
    void operator()(const daecpp::state_type &x, daecpp::state_type &f,
                    const double t)
    {
        f[0] = -0.04 * x[0] + 1.0e4 * x[1] * x[2];
        f[1] = 0.04 * x[0] - 1.0e4 * x[1] * x[2] - 3.0e7 * x[1] * x[1];
        f[2] = x[0] + x[1] + x[2] - 1;
    }
};

/*
 * (Optional) Observer
 * =============================================================================
 * Checks conservation law x1 + x2 + x3 = 1 every time step and prints solution
 * to console.
 */
class MySolver : public Solver
{
public:
    MySolver(daecpp::RHS &rhs, daecpp::Jacobian &jac, daecpp::MassMatrix &mass,
             daecpp::SolverOptions &opt)
        : daecpp::Solver(rhs, jac, mass, opt)
    {
    }

    /*
     * Overloaded observer.
     * Receives current solution vector and the current time every time step.
     */
    void observer(daecpp::state_type &x, const double t)
    {
        std::cout << " | " << x[0] << ' ' << 1e4 * x[1] << ' ' << x[2]
                  << " == " << x[0] + x[1] + x[2] - 1.0;
    }
};

/*
 * (Optional) Analytical Jacobian in 3-array sparse format
 * =============================================================================
 *
 * For more information about the sparse format see
 * https://software.intel.com/en-us/mkl-developer-reference-c-sparse-blas-csr-matrix-storage-format
 */
class MyJacobian : public Jacobian
{
public:
    MyJacobian(daecpp::RHS &rhs) : daecpp::Jacobian(rhs) {}

    /*
     * Receives current solution vector x and the current time t. Defines the
     * analytical Jacobian matrix J.
     */
    void operator()(daecpp::sparse_matrix_holder &J,
                    const daecpp::state_type &x, const double t)
    {
        // Initialize Jacobian in sparse format
        J.A.resize(9);
        J.ja.resize(9);
        J.ia.resize(4);

        // Non-zero elements
        J.A[0] = -0.04;
        J.A[1] = 1.0e4 * x[2];
        J.A[2] = 1.0e4 * x[1];
        J.A[3] = 0.04;
        J.A[4] = -1.0e4 * x[2] - 6.0e7 * x[1];
        J.A[5] = -1.0e4 * x[1];
        J.A[6] = 1.0;
        J.A[7] = 1.0;
        J.A[8] = 1.0;

        // Column index of each element given above
        J.ja[0] = 0;
        J.ja[1] = 1;
        J.ja[2] = 2;
        J.ja[3] = 0;
        J.ja[4] = 1;
        J.ja[5] = 2;
        J.ja[6] = 0;
        J.ja[7] = 1;
        J.ja[8] = 2;

        // Index of the first element for each row
        J.ia[0] = 0;
        J.ia[1] = 3;
        J.ia[2] = 6;
        J.ia[3] = 9;
    }
};

/*
 * MAIN FUNCTION
 * =============================================================================
 * Returns '0' if solution comparison is OK or '1' if solution error is above
 * acceptable tolerance.
 */
int main()
{
    // Solution time 0 <= t <= t1
    const double t1 = 4.0e6;

    // Define the state vector
    state_type x(3);

    // Initial conditions.
    // We will use slightly inconsistent initial condition to test initialization.
    x[0] = 1;
    x[1] = 0;
    x[2] = 1e-3;  // Should be 0 theoretically

    // Set up the RHS of the problem.
    // Class MyRHS inherits abstract RHS class from dae-cpp library.
    MyRHS rhs;

    // Set up the Mass Matrix of the problem.
    // MyMassMatrix inherits abstract MassMatrix class from dae-cpp library.
    MyMassMatrix mass;

    // Create an instance of the solver options and update some of the solver
    // parameters defined in solver_options.h
    SolverOptions opt;

    //////////////////////////////////// Adjust this
    opt.dt_init = 1.0e-6;  // Change initial time step
    opt.verbosity             = 2;
    opt.dt_max                = t1 / 100;
    opt.time_stepping         = 1;
    opt.dt_increase_threshold = 2;
    // opt.bdf_order = 6;

    // We can override Jacobian class from dae-cpp library and provide
    // analytical Jacobian
    MyJacobian jac(rhs);
    // jac.print(x, 0);  // print it out for t = 0

    // Or use numerically estimated one with a given tolerance
    // (commented out since we have analytical Jacobian above).
    // Jacobian jac_est(rhs, 1e-10);  // Obviously this tolerance is
                                      // inacceptable in single precision
    // jac_est.print(x, 0);           // print Jacobian out for t = 0

    // Create an instance of the solver with particular RHS, Mass matrix,
    // Jacobian and solver options
    MySolver solve(rhs, jac, mass, opt);

    // Now we are ready to solve the set of DAEs
    std::cout << "\nStarting DAE solver...\n";
    solve(x, t1);

    // Compare results with MATLAB ode15s solution
    const double x_ref[3]     = {0.00051675, 2.068e-9, 0.99948324};
    const double conservation = std::abs(x[0] + x[1] + x[2] - 1);

    // Find total relative deviation from the reference solution
    double result = 0.0;
    for(int i = 0; i < 3; i++)
        result += std::abs(x[i] - x_ref[i]) / x_ref[i] * 100;

    std::cout << "Total relative error: " << result << "%\n";
    std::cout << "Conservation law absolute deviation: " << conservation
              << '\n';

    // Plot the solution -- TODO: Update this!
#ifdef PLOTTING
    const double h = 1.0 / (double)N;

    dae::state_type_matrix x_axis, y_axis, z_axis;

    for(MKL_INT i = 0; i < N; i++)
    {
        dae::state_type x_row, y_row, z_row;

        for(MKL_INT j = 0; j < N; j++)
        {
            x_row.push_back((double)j * h + h * 0.5);
            y_row.push_back((double)i * h + h * 0.5);
            z_row.push_back(x[j + i * N]);
        }

        x_axis.push_back(x_row);
        y_axis.push_back(y_row);
        z_axis.push_back(z_row);
    }

    plt::figure();
    plt::figure_size(800, 600);
    plt::plot_surface(x_axis, y_axis, z_axis);

    // Save figure
    const char *filename = "diffusion_2d.png";
    std::cout << "Saving result to " << filename << "...\n";
    plt::save(filename);
#endif

#ifdef DAE_SINGLE
    const bool check_result = (result > 1.0 || conservation > 1e-6);
#else
    const bool check_result = (result > 1.0 || conservation > 1e-14);
#endif

    if(check_result)
        std::cout << "...Test FAILED\n\n";
    else
        std::cout << "...done\n\n";

    return check_result;
}
