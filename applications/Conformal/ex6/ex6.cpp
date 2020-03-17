/* This example is for quasi-conformal minimization */
/* mu controls the beltrami coefficient */

#include "FemusInit.hpp"
#include "MultiLevelSolution.hpp"
#include "MultiLevelProblem.hpp"
#include "NumericVector.hpp"
#include "VTKWriter.hpp"
#include "GMVWriter.hpp"
#include "NonLinearImplicitSystem.hpp"
#include "TransientSystem.hpp"
#include "adept.h"
#include <cstdlib>
#include "petsc.h"
#include "petscmat.h"
#include "PetscMatrix.hpp"

bool stopIterate = false;

using namespace femus;
// Comment back in for working code
//const double mu[2] = {0.8, 0.};

void UpdateMu (MultiLevelSolution& mlSol);

void AssembleConformalMinimization (MultiLevelProblem& ml_prob);  //stable and not bad
void AssembleShearMinimization (MultiLevelProblem& ml_prob); 

// IBVs.  No boundary, and IVs set to sphere (just need something).
bool SetBoundaryCondition (const std::vector < double >& x, const char solName[], double& value, const int faceName, const double time) {

  bool dirichlet = true;
  value = 0.;

  // if (!strcmp (solName, "Dx1")) {
  //   if (1 == faceName ) {
  //     dirichlet = false;
  //   }
  //   if (4 == faceName || 3 == faceName ) {
  //     value = (0.5 + 0.4 * cos ( (x[1] - 0.5) * acos (-1.))) * (0.5 - x[0]);
  //   }
  // }
  // else if (!strcmp (solName, "Dx2")) {
  //   if (2 == faceName) {
  //     dirichlet = false;
  //   }
  // }


  if (!strcmp (solName, "Dx1")) {
    if (1 == faceName || 3 == faceName) {
      dirichlet = true;
    }
    if (4 == faceName) {
      //value = 0.04 * sin (4*(x[1] / 0.5 * acos (-1.)));
      value = 0.5 * sin ( (x[1] / 0.5 * acos (-1.)));
      //dirichlet = false;
    }
  }
  else if (!strcmp (solName, "Dx2")) {
    if (2 == faceName) {
      dirichlet = true;
    }
  }

  // if (!strcmp (solName, "Dx2")) {
  //    if (2 == faceName || 4 == faceName) {
  //      dirichlet = false;
  //    }
  //    if (1 == faceName) {
  //      value = 0.5 * sin ((x[0] / 0.5 * acos (-1.)));
  //    }
  //  }
  //  else if (!strcmp (solName, "Dx1")) {
  //    if (3 == faceName) {
  //      dirichlet = false;
  //    }
  //  }

  return dirichlet;
}


// Main program starts here.
int main (int argc, char** args) {

  // init Petsc-MPI communicator
  FemusInit mpinit (argc, args, MPI_COMM_WORLD);


  // define multilevel mesh
  unsigned maxNumberOfMeshes;
  MultiLevelMesh mlMsh;

  // Read coarse level mesh and generate finer level meshes.
  double scalingFactor = 1.;

  //mlMsh.GenerateCoarseBoxMesh(32, 32, 0, -0.5, 0.5, -0.5, 0.5, 0., 0., QUAD9, "seventh");
  
  // Set number of mesh levels.
  //unsigned numberOfUniformLevels = 1;
  
  //mlMsh.ReadCoarseMesh ("../input/squareTri.neu", "seventh", scalingFactor);
  mlMsh.ReadCoarseMesh ("../input/square1.neu", "seventh", scalingFactor);

  // Set number of mesh levels.
  unsigned numberOfUniformLevels = 5;
  unsigned numberOfSelectiveLevels = 0;
  mlMsh.RefineMesh (numberOfUniformLevels , numberOfUniformLevels + numberOfSelectiveLevels, NULL);

  // Erase all the coarse mesh levels.
  mlMsh.EraseCoarseLevels (numberOfUniformLevels - 1);

  // print mesh info
  mlMsh.PrintInfo();

  const unsigned  dim = mlMsh.GetDimension();

  // Define the multilevel solution and attach the mlMsh object to it.
  MultiLevelSolution mlSol (&mlMsh);

  // Add variables X,Y,W to mlSol.

  mlSol.AddSolution ("Dx1", LAGRANGE, FIRST, 0);
  mlSol.AddSolution ("Dx2", LAGRANGE, FIRST, 0);

  mlSol.AddSolution ("mu1", DISCONTINUOUS_POLYNOMIAL, ZERO, 0, false);
  mlSol.AddSolution ("mu2", DISCONTINUOUS_POLYNOMIAL, ZERO, 0, false);

  mlSol.AddSolution ("smu1N", DISCONTINUOUS_POLYNOMIAL, ZERO, 0, false);
  mlSol.AddSolution ("weight1", DISCONTINUOUS_POLYNOMIAL, ZERO, 0, false);

  mlSol.AddSolution ("smu2N", LAGRANGE, FIRST, 0, false);
  mlSol.AddSolution ("weight2", LAGRANGE, FIRST, 0, false);

  // Initialize the variables and attach boundary conditions.
  mlSol.Initialize ("All");

  mlSol.AttachSetBoundaryConditionFunction (SetBoundaryCondition);
  mlSol.GenerateBdc ("All");

  MultiLevelProblem mlProb (&mlSol);

  // Add system Conformal or Shear Minimization in mlProb.
  NonLinearImplicitSystem& system = mlProb.add_system < NonLinearImplicitSystem > ("conformal"); //for conformal

  // Add solutions newDX, Lambda1 to system.
  system.AddSolutionToSystemPDE ("Dx1");
  system.AddSolutionToSystemPDE ("Dx2");

  // Parameters for convergence and # of iterations.
  system.SetMaxNumberOfNonLinearIterations (100);
  system.SetNonLinearConvergenceTolerance (1.e-10);

  system.init();
  
  mlSol.SetWriter (VTK);
  std::vector<std::string> mov_vars;
  mov_vars.push_back ("Dx1");
  mov_vars.push_back ("Dx2");
  mlSol.GetWriter()->SetMovingMesh (mov_vars);

  // and this?
  std::vector < std::string > variablesToBePrinted;
  variablesToBePrinted.push_back ("All");
  mlSol.GetWriter()->SetDebugOutput (true);
  //mlSol.GetWriter()->Write (DEFAULT_OUTPUTDIR, "linear", variablesToBePrinted, 0);
  mlSol.GetWriter()->Write (DEFAULT_OUTPUTDIR, "biquadratic", variablesToBePrinted, 0);

  // Attach the assembling function to system and initialize.
  system.SetAssembleFunction (AssembleShearMinimization);
  //system.SetAssembleFunction (AssembleConformalMinimization);
  system.MGsolve();
  mlSol.GetWriter()->Write (DEFAULT_OUTPUTDIR, "biquadratic", variablesToBePrinted, 1);
  
  system.SetAssembleFunction (AssembleConformalMinimization);
  system.MGsolve();
  
  mlSol.GetWriter()->Write (DEFAULT_OUTPUTDIR, "biquadratic", variablesToBePrinted, 2);

  return 0;
}

unsigned counter = 0;

// Building the Conformal Minimization system.
void AssembleConformalMinimization (MultiLevelProblem& ml_prob) {
  //  ml_prob is the global object from/to where get/set all the data
  //  level is the level of the PDE system to be assembled

  // call the adept stack object
  adept::Stack& s = FemusInit::_adeptStack;

  //  Extract pointers to the several objects that we are going to use.
  NonLinearImplicitSystem* mlPdeSys   = &ml_prob.get_system< NonLinearImplicitSystem> ("conformal");   // pointer to the linear implicit system named "Poisson"

  const unsigned level = mlPdeSys->GetLevelToAssemble();

  // Pointers to the mesh (level) object and elem object in mesh (level).
  Mesh *msh = ml_prob._ml_msh->GetLevel (level);
  elem *el = msh->el;

  // Pointers to the multilevel solution, solution (level) and equation (level).
  MultiLevelSolution *mlSol = ml_prob._ml_sol;

  if (counter > 0 && !stopIterate) {
    UpdateMu (*mlSol);
  }

  Solution *sol = ml_prob._ml_sol->GetSolutionLevel (level);
  LinearEquationSolver *pdeSys = mlPdeSys->_LinSolver[level];

  // Pointers to global stiffness matrix and residual vector in pdeSys (level).
  SparseMatrix *KK = pdeSys->_KK;
  NumericVector *RES = pdeSys->_RES;

  // Convenience variables to keep track of the dimension.
  const unsigned  dim = msh->GetDimension();
  const unsigned  DIM = 2;

  // Get the process_id (for parallel computation).
  unsigned iproc = msh->processor_id();

  // Setting the reference elements to be equilateral triangles.
  std::vector < std::vector < double > > xT (2);
  xT[0].resize (7);
  xT[0][0] = -0.5;
  xT[0][1] = 0.5;
  xT[0][2] = 0.;
  xT[0][3] = 0.;
  xT[0][4] = 0.25;
  xT[0][5] = -0.25;
  xT[0][6] = 0.;

  xT[1].resize (7);
  xT[1][0] = 0.;
  xT[1][1] = 0.;
  xT[1][2] = sqrt (3.) / 2.;
  xT[1][3] = 0.;
  xT[1][4] = sqrt (3.) / 4.;
  xT[1][5] = sqrt (3.) / 4.;
  xT[1][6] = sqrt (3.) / 6.;

  std::vector<double> phi_uv0;
  std::vector<double> phi_uv1;

  std::vector< double > stdVectorPhi;
  std::vector< double > stdVectorPhi_uv;

  // Extract positions of Dx in ml_sol object.
  std::vector < unsigned >  solDxIndex (DIM);
  solDxIndex[0] = mlSol->GetIndex ("Dx1");
  solDxIndex[1] = mlSol->GetIndex ("Dx2");

  std::vector < unsigned > solMuIndex (DIM);
  solMuIndex[0] = mlSol->GetIndex ("mu1");
  solMuIndex[1] = mlSol->GetIndex ("mu2");
  unsigned solType1 = mlSol->GetSolutionType (solMuIndex[0]);

  // Extract finite element type for the solution.
  unsigned solType;
  solType = mlSol->GetSolutionType (solDxIndex[0]);

  // Get the finite element type for "x", it is always 2 (LAGRANGE QUADRATIC).
  unsigned xType = 2;

  // Get the positions of Y in the pdeSys object.
  std::vector < unsigned > solDxPdeIndex (dim);
  solDxPdeIndex[0] = mlPdeSys->GetSolPdeIndex ("Dx1");
  solDxPdeIndex[1] = mlPdeSys->GetSolPdeIndex ("Dx2");

  // Local solution vectors for Nx and NDx.
  std::vector < std::vector < adept::adouble > > solDx (DIM);
  std::vector < std::vector < adept::adouble > > solx (DIM);
  std::vector < std::vector < double > > solxHat (DIM);

  std::vector < std::vector < double > > solMu (DIM);

  // Local-to-global pdeSys dofs.
  std::vector < int > SYSDOF;

  // Local residual vectors.
  vector< double > Res;
  std::vector < std::vector< adept::adouble > > aResDx (dim);

  // Local Jacobian matrix (ordered by column).
  vector < double > Jac;

  KK->zero();  // Zero all the entries of the Global Matrix
  RES->zero(); // Zero all the entries of the Global Residual

  // ELEMENT LOOP: each process loops only on the elements that it owns.
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    // Numer of solution element dofs.
    short unsigned ielGeom = msh->GetElementType (iel);
    unsigned nxDofs  = msh->GetElementDofNumber (iel, solType);
    unsigned nDofs1  = msh->GetElementDofNumber (iel, solType1);

    // Resize local arrays.
    for (unsigned K = 0; K < DIM; K++) {
      solDx[K].resize (nxDofs);
      solx[K].resize (nxDofs);
      solxHat[K].resize (nxDofs);
      solMu[K].resize (nDofs1);
    }

    // Resize local arrays
    SYSDOF.resize (dim * nxDofs);
    Res.resize (dim * nxDofs);

    for (unsigned k = 0; k < dim; k++) {
      aResDx[k].assign (nxDofs, 0.);
    }

    // local storage of global mapping and solution
    for (unsigned i = 0; i < nxDofs; i++) {
      // Global-to-local mapping between X solution node and solution dof.
      unsigned iDDof = msh->GetSolutionDof (i, iel, solType);
      for (unsigned K = 0; K < DIM; K++) {
        solDx[K][i] = (*sol->_Sol[solDxIndex[K]]) (iDDof);
        // Global-to-global mapping between NDx solution node and pdeSys dof.
        if (K < dim) {
          SYSDOF[ K * nxDofs + i] = pdeSys->GetSystemDof (solDxIndex[K], solDxPdeIndex[K], i, iel);
        }
      }
    }

    for (unsigned i = 0; i < nDofs1; i++) {
      unsigned iDof = msh->GetSolutionDof (i, iel, solType1);
      for (unsigned K = 0; K < DIM; K++) {
        solMu[K][i] = (*sol->_Sol[solMuIndex[K]]) (iDof);
      }
    }



    // start a new recording of all the operations involving adept variables.
    s.new_recording();
    for (unsigned i = 0; i < nxDofs; i++) {
      unsigned iXDof  = msh->GetSolutionDof (i, iel, xType);
      for (unsigned K = 0; K < DIM; K++) {
        solxHat[K][i] = (*msh->_topology->_Sol[K]) (iXDof);
        solx[K][i] = (*msh->_topology->_Sol[K]) (iXDof) + solDx[K][i];
      }
    }

    // *** Gauss point loop ***
    for (unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solType]->GetGaussPointNumber(); ig++) {

      const double *phix;  // local test function
      const double *phi1;  // local test function
      const double *phix_uv[dim]; // local test function first order partial derivatives

      double weight; // gauss point weight

      // Get Gauss point weight, test function, and first order derivatives.
      if (ielGeom == QUAD) {
        phix = msh->_finiteElement[ielGeom][solType]->GetPhi (ig);

        phix_uv[0] = msh->_finiteElement[ielGeom][solType]->GetDPhiDXi (ig);
        phix_uv[1] = msh->_finiteElement[ielGeom][solType]->GetDPhiDEta (ig);

        weight = msh->_finiteElement[ielGeom][solType]->GetGaussWeight (ig);
      }

      // Special adjustments for triangles.
      else {
        msh->_finiteElement[ielGeom][solType]->Jacobian (xT, ig, weight, stdVectorPhi, stdVectorPhi_uv);
        phix = &stdVectorPhi[0];
        phi_uv0.resize (nxDofs);
        phi_uv1.resize (nxDofs);
        for (unsigned i = 0; i < nxDofs; i++) {
          phi_uv0[i] = stdVectorPhi_uv[i * dim];
          phi_uv1[i] = stdVectorPhi_uv[i * dim + 1];
        }
        phix_uv[0] = &phi_uv0[0];
        phix_uv[1] = &phi_uv1[0];
      }

      phi1 = msh->_finiteElement[ielGeom][solType1]->GetPhi (ig);

      // Initialize and compute values of x, Dx, NDx, x_uv at the Gauss points.
      double solxHatg[DIM] = {0., 0.};
      adept::adouble solx_uv[2][2] = {{0., 0.}, {0., 0.}};
      for (unsigned K = 0; K < DIM; K++) {
        for (unsigned i = 0; i < nxDofs; i++) {
          solxHatg[K] += phix[i] * solxHat[K][i];
        }
        for (int j = 0; j < dim; j++) {
          for (unsigned i = 0; i < nxDofs; i++) {
            solx_uv[K][j] += phix_uv[j][i] * solx[K][i];
          }
        }
        // for (unsigned i = 0; i < nxDofs; i++) {
        //   solx_z[K] +=
        //   solx_zBar[K] +=
        // }
      }

      // Compute the metric, metric determinant, and area element.
      std::vector < std::vector < adept::adouble > > g (dim);
      for (unsigned i = 0; i < dim; i++) g[i].assign (dim, 0.);

      for (unsigned i = 0; i < dim; i++) {
        for (unsigned j = 0; j < dim; j++) {
          for (unsigned K = 0; K < DIM; K++) {
            g[i][j] += solx_uv[K][i] * solx_uv[K][j];
          }
        }
      }

      adept::adouble detg = g[0][0] * g[1][1] - g[0][1] * g[1][0];
      adept::adouble Area = weight * sqrt (detg);
      adept::adouble Area2 = weight;// Trick to give equal weight to each element.

//       adept::adouble norm2Xz = (1. / 4.) * (pow ( (solx_uv[0][0] + solx_uv[1][1]), 2) + pow ( (solx_uv[1][0] - solx_uv[0][1]), 2));
//
//       // Discretize the equation \delta CD = 0 on the basis d/du, d/dv.
//       adept::adouble XzBarXz_Bar[DIM];
//
//       // Comment out for working code
//
//
//       XzBarXz_Bar[0] = (1. / 4.) * (pow (solx_uv[0][0], 2) + pow (solx_uv[1][0], 2) - pow (solx_uv[0][1], 2) - pow (solx_uv[1][1], 2));
//       XzBarXz_Bar[1] = (1. / 2.) * (solx_uv[0][0] * solx_uv[0][1] + solx_uv[1][0] * solx_uv[1][1]);
//
//       // Comment out for working code

      double mu[2] = {0., 0.};

      for (unsigned i = 0; i < nDofs1; i++) {
        for (unsigned K = 0; K < DIM; K++) {
          mu[K] += phi1[i] * solMu[K][i];
        }
      }



//       if (counter == 0) mu[0] = 0.8;
//
//       for (unsigned K = 0; K < DIM; K++) {
//         if (counter > 0 && norm2Xz.value() > 0.) {
//           mu[K] += (1. / norm2Xz.value()) * XzBarXz_Bar[K].value();
//         }
//         //if(counter % 2 == 0) mu[K]*=1.01;
//         //else mu[K]/=1.01;
//       }

      //std::cout << mu[0] <<" "<< mu[1]<<" ";

      adept::adouble V[DIM];
      V[0] = (1 - mu[0]) * solx_uv[0][0] - (1 + mu[0]) * solx_uv[1][1] + mu[1] * (solx_uv[1][0] - solx_uv[0][1]);
      V[1] = (1 - mu[0]) * solx_uv[1][0] + (1 + mu[0]) * solx_uv[0][1] - mu[1] * (solx_uv[0][0] + solx_uv[1][1]);


      adept::adouble M[DIM][dim];

      M[0][0] = (1 - mu[0]) * V[0] - mu[1] * V[1];
      M[1][0] = (1 - mu[0]) * V[1] + mu[1] * V[0];
      //M[0][0] = (1 - mu1) * V[0] - mu2 * V[1];
      //M[1][0] = (1 - mu1) * V[1] + mu2 * V[0];

      M[0][1] = (1 + mu[0]) * V[1] - mu[1] * V[0];
      M[1][1] = - (1 + mu[0]) * V[0] - mu[1] * V[1];
      //M[0][1] = (1 + mu1) * V[1] - mu2 * V[0];
      //M[1][1]= -(1 + mu1) * V[0] - mu2 * V[1];


      // Implement the Conformal Minimization equations.
      for (unsigned k = 0; k < dim; k++) {
        for (unsigned i = 0; i < nxDofs; i++) {
          adept::adouble term1 = 0.;
          for (unsigned j = 0; j < dim; j++) {
            term1 += 2 * M[k][j] * phix_uv[j][i];
          }
          // Conformal energy equation (with trick).
          aResDx[k][i] += term1 * Area2;
        }
      }
    } // end GAUSS POINT LOOP

    //------------------------------------------------------------------------
    // Add the local Matrix/Vector into the global Matrix/Vector
    //copy the value of the adept::adoube aRes in double Res and store

    for (int k = 0; k < dim; k++) {
      for (int i = 0; i < nxDofs; i++) {
        Res[ k * nxDofs + i] = -aResDx[k][i].value();
      }
    }

    RES->add_vector_blocked (Res, SYSDOF);

    // Resize Jacobian.
    Jac.resize ( (dim * nxDofs) * (dim * nxDofs));

    // Define the dependent variables.
    for (int k = 0; k < dim; k++) {
      s.dependent (&aResDx[k][0], nxDofs);
    }

    // Define the independent variables.
    for (int k = 0; k < dim; k++) {
      s.independent (&solDx[k][0], nxDofs);
    }

    // Get the jacobian matrix (ordered by row).
    s.jacobian (&Jac[0], true);

    KK->add_matrix_blocked (Jac, SYSDOF, SYSDOF);

    s.clear_independents();
    s.clear_dependents();

  } //end ELEMENT LOOP for each process.

  RES->close();
  KK->close();

  counter++;

} // end AssembleConformalMinimization.

void UpdateMu (MultiLevelSolution& mlSol) {

  //MultiLevelSolution*  mlSol = ml_prob._ml_sol;
  unsigned level = mlSol._mlMesh->GetNumberOfLevels() - 1u;

  Solution* sol = mlSol.GetSolutionLevel (level);
  Mesh* msh = mlSol._mlMesh->GetLevel (level);
  elem* el = msh->el;

  unsigned  dim = msh->GetDimension();

  std::vector < unsigned > solDx (dim);
  solDx[0] = mlSol.GetIndex ("Dx1");
  solDx[1] = mlSol.GetIndex ("Dx2");
  unsigned solTypeDx = mlSol.GetSolutionType (solDx[0]);

  std::vector < unsigned > solmu (dim);
  solmu[0] = mlSol.GetIndex ("mu1");
  solmu[1] = mlSol.GetIndex ("mu2");
  
  unsigned solsmu1N = mlSol.GetIndex ("smu1N");

  unsigned solw1 = mlSol.GetIndex ("weight1");
  unsigned solType1 = mlSol.GetSolutionType (solmu[0]);

  unsigned iproc = msh->processor_id();
  unsigned nprocs = msh->n_processors();

  std::vector<double> dof1;

  double weight2;
  std::vector <double> phi2;
  std::vector <double> phi2_x;

  std::vector < std::vector < double > > x (dim);

  for (unsigned k = 0; k < dim; k++) {
    sol->_Sol[solmu[k]]->zero();
  }
  sol->_Sol[solw1]->zero();


  //For triangle elements
  std::vector<double> phi_uv0;
  std::vector<double> phi_uv1;
  std::vector< double > stdVectorPhi;
  std::vector< double > stdVectorPhi_uv;
  std::vector < std::vector < double > > xT (2);
  xT[0].resize (7);
  xT[0][0] = -0.5;
  xT[0][1] = 0.5;
  xT[0][2] = 0.;
  xT[0][3] = 0.;
  xT[0][4] = 0.25;
  xT[0][5] = -0.25;
  xT[0][6] = 0.;

  xT[1].resize (7);
  xT[1][0] = 0.;
  xT[1][1] = 0.;
  xT[1][2] = sqrt (3.) / 2.;
  xT[1][3] = 0.;
  xT[1][4] = sqrt (3.) / 4.;
  xT[1][5] = sqrt (3.) / 4.;
  xT[1][6] = sqrt (3.) / 6.;


  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    short unsigned ielGeom = msh->GetElementType (iel);
    unsigned nDofs1  = msh->GetElementDofNumber (iel, solType1);
    unsigned nDofsDx  = msh->GetElementDofNumber (iel, solTypeDx);

    dof1.resize (nDofs1);

    for (int k = 0; k < dim; k++) {
      x[k].resize (nDofsDx);
    }

    // local storage of global mapping and solution
    for (unsigned i = 0; i < nDofs1; i++) {
      dof1[i] = msh->GetSolutionDof (i, iel, solType1);
    }
    // local storage of coordinates
    for (unsigned i = 0; i < nDofsDx; i++) {
      unsigned idof = msh->GetSolutionDof (i, iel, solTypeDx);
      unsigned xDof  = msh->GetSolutionDof (i, iel, 2);
      for (unsigned k = 0; k < dim; k++) {
        x[k][i] = (*msh->_topology->_Sol[k]) (xDof) + (*sol->_Sol[solDx[k]]) (idof);
      }
    }

    for (unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solTypeDx]->GetGaussPointNumber(); ig++) {

      const double *phi;  // local test function
      const double *phi_uv[dim]; // local test function first order partial derivatives
      double weight; // gauss point weight

      double *phi1 = msh->_finiteElement[ielGeom][solType1]->GetPhi (ig);

      // Get Gauss point weight, test function, and first order derivatives.
      if (ielGeom == QUAD) {
        //phi = msh->_finiteElement[ielGeom][solTypeDx]->GetPhi (ig);
        phi_uv[0] = msh->_finiteElement[ielGeom][solTypeDx]->GetDPhiDXi (ig);
        phi_uv[1] = msh->_finiteElement[ielGeom][solTypeDx]->GetDPhiDEta (ig);
        weight = msh->_finiteElement[ielGeom][solTypeDx]->GetGaussWeight (ig);
      }

      // Special adjustments for triangles.
      else {
        msh->_finiteElement[ielGeom][solTypeDx]->Jacobian (xT, ig, weight, stdVectorPhi, stdVectorPhi_uv);
        phi_uv0.resize (nDofsDx);
        phi_uv1.resize (nDofsDx);
        for (unsigned i = 0; i < nDofsDx; i++) {
          phi_uv0[i] = stdVectorPhi_uv[i * dim];
          phi_uv1[i] = stdVectorPhi_uv[i * dim + 1];
        }
        phi_uv[0] = &phi_uv0[0];
        phi_uv[1] = &phi_uv1[0];
      }

      // Initialize and compute values of x, Dx, NDx, x_uv at the Gauss points.
      double solx_uv[2][2] = {{0., 0.}, {0., 0.}};
      for (unsigned k = 0; k < dim; k++) {
        for (int j = 0; j < dim; j++) {
          for (unsigned i = 0; i < nDofsDx; i++) {
            solx_uv[k][j] += phi_uv[j][i] * x[k][i];
          }
        }
      }

      double norm2Xz = (1. / 4.) * (pow ( (solx_uv[0][0] + solx_uv[1][1]), 2) + pow ( (solx_uv[1][0] - solx_uv[0][1]), 2));
      double XzBarXz_Bar[2];

      XzBarXz_Bar[0] = (1. / 4.) * (pow (solx_uv[0][0], 2) + pow (solx_uv[1][0], 2) - pow (solx_uv[0][1], 2) - pow (solx_uv[1][1], 2));
      XzBarXz_Bar[1] = (1. / 2.) * (solx_uv[0][0] * solx_uv[0][1] + solx_uv[1][0] * solx_uv[1][1]);

      // Comment out for working code

      double mu[2] = {0., 0.};
      for (unsigned k = 0; k < 2; k++) {
        if (norm2Xz > 0.) {
          mu[k] += (1. / norm2Xz) * XzBarXz_Bar[k];
        }
      }

      for (unsigned i = 0; i < nDofs1; i++) {
        sol->_Sol[solw1]->add (dof1[i], phi1[i] * weight);
        for (unsigned k = 0; k < dim; k++) {
          sol->_Sol[solmu[k]]->add (dof1[i], mu[k] * phi1[i] * weight);
        }
      } // end phi_i loop

    } // end gauss point loop

  } //end element loop for each process*/

  for (unsigned k = 0; k < dim; k++) {
    sol->_Sol[solmu[k]]->close();
  }
  sol->_Sol[solw1]->close();

  for (unsigned i = msh->_dofOffset[solType1][iproc]; i < msh->_dofOffset[solType1][iproc + 1]; i++) {

    double weight = (*sol->_Sol[solw1]) (i);

    double mu[2];
    for (unsigned k = 0; k < dim; k++) {
      mu[k] = (*sol->_Sol[solmu[k]]) (i);
      sol->_Sol[solmu[k]]->set (i, mu[k] / weight);
    }
    sol->_Sol[solsmu1N]->set (i, sqrt( mu[0] * mu[0] + mu[1] * mu[1]) / weight);
  }
  for (unsigned k = 0; k < dim; k++) {
    sol->_Sol[solmu[k]]->close();
  }
  sol->_Sol[solsmu1N]->close();
  
  double norm = sol->_Sol[solsmu1N]->linfty_norm();
  std::cout << norm << std::endl;
  if ( norm < 0.5) stopIterate = true;

  /////////////////////////////////////////////////
   
  double MuNormLocalSum = 0.;
  for (unsigned i = msh->_dofOffset[solType1][iproc]; i < msh->_dofOffset[solType1][iproc + 1]; i++) {
    double muN = 0.;
    for (unsigned k = 0; k < dim; k++) {
      double muk = (*sol->_Sol[solmu[k]]) (i);
      muN += muk * muk;
    }
    MuNormLocalSum += sqrt (muN);
  }
  
  double MuNormAverage;
  MPI_Allreduce(&MuNormLocalSum, &MuNormAverage, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  MuNormAverage /= msh->_dofOffset[solType1][nprocs];
  
  for (unsigned i = msh->_dofOffset[solType1][iproc]; i < msh->_dofOffset[solType1][iproc + 1]; i++) {

    double mu[2];
    double normMu = 0.;
    for (unsigned k = 0; k < dim; k++) {
      mu[k] = (*sol->_Sol[solmu[k]]) (i);
      normMu += mu[k] * mu[k];
    }
    normMu = sqrt (normMu);
    for (unsigned k = 0; k < dim; k++) {
      sol->_Sol[solmu[k]]->set (i, MuNormAverage * mu[k] / normMu);
    }
    
  }

  for (unsigned k = 0; k < dim; k++) {
    sol->_Sol[solmu[k]]->close();
  }

}


void AssembleShearMinimization (MultiLevelProblem& ml_prob) {
  //  ml_prob is the global object from/to where get/set all the data
  //  level is the level of the PDE system to be assembled
  //  levelMax is the Maximum level of the MultiLevelProblem
  //  assembleMatrix is a flag that tells if only the residual or also the matrix should be assembled


  // call the adept stack object
  adept::Stack& s = FemusInit::_adeptStack;

  //  extract pointers to the several objects that we are going to use
  LinearImplicitSystem* mlPdeSys   = &ml_prob.get_system< LinearImplicitSystem> ("conformal");   // pointer to the linear implicit system named "Poisson"

  const unsigned level = mlPdeSys->GetLevelToAssemble();

  Mesh *msh = ml_prob._ml_msh->GetLevel (level);   // pointer to the mesh (level) object
  elem *el = msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution *mlSol = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution *sol = ml_prob._ml_sol->GetSolutionLevel (level);   // pointer to the solution (level) object
  LinearEquationSolver *pdeSys = mlPdeSys->_LinSolver[level]; // pointer to the equation (level) object

  SparseMatrix *KK = pdeSys->_KK;  // pointer to the global stiffness matrix object in pdeSys (level)
  NumericVector *RES = pdeSys->_RES; // pointer to the global residual vector object in pdeSys (level)

  const unsigned  dim = msh->GetDimension();

  std::vector <double> phi;  // local test function for velocity
  std::vector <adept::adouble> phi_x; // local test function first order partial derivatives
  adept::adouble weight; // gauss point weight

  unsigned iproc = msh->processor_id(); // get the process_id (for parallel computation)

  //solution variable
  std::vector < unsigned > solDxIndex (dim);
  solDxIndex[0] = mlSol->GetIndex ("Dx1"); // get the position of "DX" in the ml_sol object
  solDxIndex[1] = mlSol->GetIndex ("Dx2"); // get the position of "DY" in the ml_sol object
  if (dim == 3) solDxIndex[2] = mlSol->GetIndex ("Dx3"); // get the position of "DY" in the ml_sol object

  unsigned solType;
  solType = mlSol->GetSolutionType (solDxIndex[0]);  // get the finite element type for "U"

  unsigned xType = 2; // get the finite element type for "x", it is always 2 (LAGRANGE QUADRATIC)

  std::vector < unsigned > solDxPdeIndex (dim);
  solDxPdeIndex[0] = mlPdeSys->GetSolPdeIndex ("Dx1");   // get the position of "Dx1" in the pdeSys object
  solDxPdeIndex[1] = mlPdeSys->GetSolPdeIndex ("Dx2");   // get the position of "Dx2" in the pdeSys object
  if (dim == 3) solDxPdeIndex[2] = mlPdeSys->GetSolPdeIndex ("Dx3");  // get the position of "Dx3" in the pdeSys object

  std::vector < std::vector < adept::adouble > > solDx (dim); // local Y solution
  std::vector < std::vector < adept::adouble > > x (dim);

  std::vector< int > SYSDOF; // local to global pdeSys dofs

  vector< double > Res; // local redidual vector
  std::vector< adept::adouble > aResDx[dim]; // local redidual vector

  vector < double > Jac; // local Jacobian matrix (ordered by column, adept)

  KK->zero();  // Set to zero all the entries of the Global Matrix
  RES->zero(); // Set to zero all the entries of the Global Residual

  // element loop: each process loops only on the elements that owns
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    short unsigned ielGeom = msh->GetElementType (iel);
    unsigned nxDofs  = msh->GetElementDofNumber (iel, solType);   // number of solution element dofs

    for (unsigned k = 0; k < dim; k++) {
      solDx[k].resize (nxDofs);
      x[k].resize (nxDofs);
    }

    // resize local arrays
    SYSDOF.resize (dim * nxDofs);
    Res.resize (dim * nxDofs);       //resize

    for (unsigned k = 0; k < dim; k++) {
      aResDx[k].assign (nxDofs, 0.);  //resize and zet to zero
    }


    // local storage of global mapping and solution
    for (unsigned i = 0; i < nxDofs; i++) {
      // Global-to-local mapping between X solution node and solution dof.
      unsigned iDDof = msh->GetSolutionDof (i, iel, solType);
      for (unsigned k = 0; k < dim; k++) {
        solDx[k][i] = (*sol->_Sol[solDxIndex[k]]) (iDDof);
        // Global-to-global mapping between NDx solution node and pdeSys dof.
        SYSDOF[ k * nxDofs + i] = pdeSys->GetSystemDof (solDxIndex[k], solDxPdeIndex[k], i, iel);
      }
    }

    // start a new recording of all the operations involving adept variables.
    s.new_recording();
    for (unsigned i = 0; i < nxDofs; i++) {
      unsigned iXDof  = msh->GetSolutionDof (i, iel, xType);
      for (unsigned k = 0; k < dim; k++) {
        x[k][i] = (*msh->_topology->_Sol[k]) (iXDof);
      }
    }

    // *** Gauss point loop ***
    for (unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solType]->GetGaussPointNumber(); ig++) {

      msh->_finiteElement[ielGeom][solType]->Jacobian (x, ig, weight, phi, phi_x);

      std::vector < std::vector < adept::adouble > > gradSolDx (dim);

      for (unsigned  k = 0; k < dim; k++) {
        gradSolDx[k].assign (dim, 0.);
      }

      for (unsigned i = 0; i < nxDofs; i++) {
        for (unsigned j = 0; j < dim; j++) {
          for (unsigned  k = 0; k < dim; k++) {
            gradSolDx[k][j] += (x[k][i] + solDx[k][i]) * phi_x[i * dim + j];
          }
        }
      }

      for (unsigned i = 0; i < nxDofs; i++) {
        for (unsigned  k = 0; k < dim; k++) {
          adept::adouble term = 0.;
          term  +=  phi_x[i * dim + k] * (gradSolDx[k][k]);
          aResDx[k][i] += term * weight;
        }
      }
    } // end gauss point loop

    //--------------------------------------------------------------------------------------------------------
    // Add the local Matrix/Vector into the global Matrix/Vector

    //copy the value of the adept::adoube aRes in double Res and store


    for (int k = 0; k < dim; k++) {
      for (int i = 0; i < nxDofs; i++) {
        Res[ k * nxDofs + i] = -aResDx[k][i].value();
      }
    }

    RES->add_vector_blocked (Res, SYSDOF);

    Jac.resize ( (dim * nxDofs) * (dim * nxDofs));

    // define the dependent variables

    for (int k = 0; k < dim; k++) {
      s.dependent (&aResDx[k][0], nxDofs);
    }

    // define the dependent variables

    for (int k = 0; k < dim; k++) {
      s.independent (&solDx[k][0], nxDofs);
    }

    // get the jacobian matrix (ordered by row)
    s.jacobian (&Jac[0], true);

    KK->add_matrix_blocked (Jac, SYSDOF, SYSDOF);

    s.clear_independents();
    s.clear_dependents();

  } //end element loop for each process

  RES->close();
  KK->close();
  
  counter++;

  // ***************** END ASSEMBLY *******************
}

void UpdateMuOld (MultiLevelSolution& mlSol) {

  //MultiLevelSolution*  mlSol = ml_prob._ml_sol;
  unsigned level = mlSol._mlMesh->GetNumberOfLevels() - 1u;

  Solution* sol = mlSol.GetSolutionLevel (level);
  Mesh* msh = mlSol._mlMesh->GetLevel (level);
  elem* el = msh->el;

  unsigned  dim = msh->GetDimension();

  std::vector < unsigned > solDx (dim);
  solDx[0] = mlSol.GetIndex ("Dx1");
  solDx[1] = mlSol.GetIndex ("Dx2");
  unsigned solTypeDx = mlSol.GetSolutionType (solDx[0]);

  std::vector < unsigned > solmu (dim);
  solmu[0] = mlSol.GetIndex ("mu1");
  solmu[1] = mlSol.GetIndex ("mu2");
  
  unsigned solsmu1N = mlSol.GetIndex ("smu1N");

  unsigned solw1 = mlSol.GetIndex ("weight1");
  unsigned solType1 = mlSol.GetSolutionType (solmu[0]);

  unsigned iproc = msh->processor_id();

  std::vector<double> dof1;

  double weight2;
  std::vector <double> phi2;
  std::vector <double> phi2_x;

  std::vector < std::vector < double > > x (dim);

  for (unsigned k = 0; k < dim; k++) {
    sol->_Sol[solmu[k]]->zero();
  }
  sol->_Sol[solw1]->zero();


  //For triangle elements
  std::vector<double> phi_uv0;
  std::vector<double> phi_uv1;
  std::vector< double > stdVectorPhi;
  std::vector< double > stdVectorPhi_uv;
  std::vector < std::vector < double > > xT (2);
  xT[0].resize (7);
  xT[0][0] = -0.5;
  xT[0][1] = 0.5;
  xT[0][2] = 0.;
  xT[0][3] = 0.;
  xT[0][4] = 0.25;
  xT[0][5] = -0.25;
  xT[0][6] = 0.;

  xT[1].resize (7);
  xT[1][0] = 0.;
  xT[1][1] = 0.;
  xT[1][2] = sqrt (3.) / 2.;
  xT[1][3] = 0.;
  xT[1][4] = sqrt (3.) / 4.;
  xT[1][5] = sqrt (3.) / 4.;
  xT[1][6] = sqrt (3.) / 6.;


  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    short unsigned ielGeom = msh->GetElementType (iel);
    unsigned nDofs1  = msh->GetElementDofNumber (iel, solType1);
    unsigned nDofsDx  = msh->GetElementDofNumber (iel, solTypeDx);

    dof1.resize (nDofs1);

    for (int k = 0; k < dim; k++) {
      x[k].resize (nDofsDx);
    }

    // local storage of global mapping and solution
    for (unsigned i = 0; i < nDofs1; i++) {
      dof1[i] = msh->GetSolutionDof (i, iel, solType1);
    }
    // local storage of coordinates
    for (unsigned i = 0; i < nDofsDx; i++) {
      unsigned idof = msh->GetSolutionDof (i, iel, solTypeDx);
      unsigned xDof  = msh->GetSolutionDof (i, iel, 2);
      for (unsigned k = 0; k < dim; k++) {
        x[k][i] = (*msh->_topology->_Sol[k]) (xDof) + (*sol->_Sol[solDx[k]]) (idof);
      }
    }

    for (unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solTypeDx]->GetGaussPointNumber(); ig++) {

      const double *phi;  // local test function
      const double *phi_uv[dim]; // local test function first order partial derivatives
      double weight; // gauss point weight

      double *phi1 = msh->_finiteElement[ielGeom][solType1]->GetPhi (ig);

      // Get Gauss point weight, test function, and first order derivatives.
      if (ielGeom == QUAD) {
        //phi = msh->_finiteElement[ielGeom][solTypeDx]->GetPhi (ig);
        phi_uv[0] = msh->_finiteElement[ielGeom][solTypeDx]->GetDPhiDXi (ig);
        phi_uv[1] = msh->_finiteElement[ielGeom][solTypeDx]->GetDPhiDEta (ig);
        weight = msh->_finiteElement[ielGeom][solTypeDx]->GetGaussWeight (ig);
      }

      // Special adjustments for triangles.
      else {
        msh->_finiteElement[ielGeom][solTypeDx]->Jacobian (xT, ig, weight, stdVectorPhi, stdVectorPhi_uv);
        phi_uv0.resize (nDofsDx);
        phi_uv1.resize (nDofsDx);
        for (unsigned i = 0; i < nDofsDx; i++) {
          phi_uv0[i] = stdVectorPhi_uv[i * dim];
          phi_uv1[i] = stdVectorPhi_uv[i * dim + 1];
        }
        phi_uv[0] = &phi_uv0[0];
        phi_uv[1] = &phi_uv1[0];
      }

      // Initialize and compute values of x, Dx, NDx, x_uv at the Gauss points.
      double solx_uv[2][2] = {{0., 0.}, {0., 0.}};
      for (unsigned k = 0; k < dim; k++) {
        for (int j = 0; j < dim; j++) {
          for (unsigned i = 0; i < nDofsDx; i++) {
            solx_uv[k][j] += phi_uv[j][i] * x[k][i];
          }
        }
      }

      double norm2Xz = (1. / 4.) * (pow ( (solx_uv[0][0] + solx_uv[1][1]), 2) + pow ( (solx_uv[1][0] - solx_uv[0][1]), 2));
      double XzBarXz_Bar[2];

      XzBarXz_Bar[0] = (1. / 4.) * (pow (solx_uv[0][0], 2) + pow (solx_uv[1][0], 2) - pow (solx_uv[0][1], 2) - pow (solx_uv[1][1], 2));
      XzBarXz_Bar[1] = (1. / 2.) * (solx_uv[0][0] * solx_uv[0][1] + solx_uv[1][0] * solx_uv[1][1]);

      // Comment out for working code

      double mu[2] = {0., 0.};
      for (unsigned k = 0; k < 2; k++) {
        if (norm2Xz > 0.) {
          mu[k] += (1. / norm2Xz) * XzBarXz_Bar[k];
        }
      }

      for (unsigned i = 0; i < nDofs1; i++) {
        sol->_Sol[solw1]->add (dof1[i], phi1[i] * weight);
        for (unsigned k = 0; k < dim; k++) {
          sol->_Sol[solmu[k]]->add (dof1[i], mu[k] * phi1[i] * weight);
        }
      } // end phi_i loop

    } // end gauss point loop

  } //end element loop for each process*/

  for (unsigned k = 0; k < dim; k++) {
    sol->_Sol[solmu[k]]->close();
  }
  sol->_Sol[solw1]->close();

  for (unsigned i = msh->_dofOffset[solType1][iproc]; i < msh->_dofOffset[solType1][iproc + 1]; i++) {

    double weight = (*sol->_Sol[solw1]) (i);

    double mu[2];
    for (unsigned k = 0; k < dim; k++) {
      mu[k] = (*sol->_Sol[solmu[k]]) (i);
      sol->_Sol[solmu[k]]->set (i, mu[k] / weight);
    }
    sol->_Sol[solsmu1N]->set (i, sqrt( mu[0] * mu[0] + mu[1] * mu[1]) / weight);
  }
  for (unsigned k = 0; k < dim; k++) {
    sol->_Sol[solmu[k]]->close();
  }
  sol->_Sol[solsmu1N]->close();
  
  double norm = sol->_Sol[solsmu1N]->linfty_norm();
  std::cout << norm << std::endl;
  if ( norm < 0.5) stopIterate = true;

/////////////////////////////////////////////////

  

  unsigned solw2 = mlSol.GetIndex ("weight2");
  unsigned solsmu2N = mlSol.GetIndex ("smu2N"); //smooth ni norm

  unsigned solType2 = mlSol.GetSolutionType (solsmu2N);

  std::vector<double> dof2;
  std::vector<double> sol1;

  sol->_Sol[solsmu2N]->zero();
  sol->_Sol[solw2]->zero();

  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    short unsigned ielGeom = msh->GetElementType (iel);
    unsigned nDofs1  = msh->GetElementDofNumber (iel, solType1);
    unsigned nDofs2  = msh->GetElementDofNumber (iel, solType2);

    sol1.resize (nDofs1);
    dof2.resize (nDofs2);

    for (int k = 0; k < dim; k++) {
      x[k].resize (nDofs2);
    }

    for (unsigned i = 0; i < nDofs1; i++) {
      unsigned idof = msh->GetSolutionDof (i, iel, solType1);
      double muN = 0.;
      for (unsigned k = 0; k < dim; k++) {
        double muk = (*sol->_Sol[solmu[k]]) (idof);
        muN += muk * muk;
      }
      sol1[i] = sqrt (muN);
    }

    // local storage of global mapping and solution
    for (unsigned i = 0; i < nDofs2; i++) {
      dof2[i] = msh->GetSolutionDof (i, iel, solType2);
    }
    // local storage of coordinates
    for (unsigned i = 0; i < nDofs2; i++) {
      unsigned xDof  = msh->GetSolutionDof (i, iel, 2);
      for (unsigned k = 0; k < dim; k++) {
        x[k][i] = (*msh->_topology->_Sol[k]) (xDof);
      }
    }

    for (unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solType2]->GetGaussPointNumber(); ig++) {
      msh->_finiteElement[ielGeom][solType2]->Jacobian (x, ig, weight2, phi2, phi2_x);

      double *phi1 = msh->_finiteElement[ielGeom][solType1]->GetPhi (ig);
      double sol1g = 0.;
      for (unsigned i = 0; i < nDofs1; i++) {
        sol1g += phi1[i] * sol1[i];
      }

      // *** phi_i loop ***
      for (unsigned i = 0; i < nDofs2; i++) {
        sol->_Sol[solw2]->add (dof2[i], phi2[i] * weight2);
        sol->_Sol[solsmu2N]->add (dof2[i], sol1g * phi2[i] * weight2);
      } // end phi_i loop
    } // end gauss point loop

  } //end element loop for each process*/
  sol->_Sol[solw2]->close();
  sol->_Sol[solsmu2N]->close();

  for (unsigned i = msh->_dofOffset[solType2][iproc]; i < msh->_dofOffset[solType2][iproc + 1]; i++) {
    double value = (*sol->_Sol[solsmu2N]) (i);
    double weight = (*sol->_Sol[solw2]) (i);
    sol->_Sol[solsmu2N]->set (i, value / weight);
  }
  sol->_Sol[solsmu2N]->close();

/////////////////////////////////////////////////

  std::vector<double> sol2;

  sol->_Sol[solsmu1N]->zero();
  sol->_Sol[solw1]->zero();

  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    short unsigned ielGeom = msh->GetElementType (iel);
    unsigned nDofs1  = msh->GetElementDofNumber (iel, solType1);
    unsigned nDofs2  = msh->GetElementDofNumber (iel, solType2);

    sol2.resize (nDofs2);
    dof1.resize (nDofs1);

    for (int k = 0; k < dim; k++) {
      x[k].resize (nDofs2);
    }

    for (unsigned i = 0; i < nDofs2; i++) {
      unsigned idof = msh->GetSolutionDof (i, iel, solType2);
      sol2[i] = (*sol->_Sol[solsmu2N]) (idof);
    }

    // local storage of global mapping and solution
    for (unsigned i = 0; i < nDofs1; i++) {
      dof1[i] = msh->GetSolutionDof (i, iel, solType1);
    }
    // local storage of coordinates
    for (unsigned i = 0; i < nDofs2; i++) {
      unsigned xDof  = msh->GetSolutionDof (i, iel, 2);
      for (unsigned k = 0; k < dim; k++) {
        x[k][i] = (*msh->_topology->_Sol[k]) (xDof);
      }
    }

    for (unsigned ig = 0; ig < msh->_finiteElement[ielGeom][solType2]->GetGaussPointNumber(); ig++) {
      msh->_finiteElement[ielGeom][solType2]->Jacobian (x, ig, weight2, phi2, phi2_x);

      double *phi1 = msh->_finiteElement[ielGeom][solType1]->GetPhi (ig);

      double sol2g = 0.;
      for (unsigned i = 0; i < nDofs2; i++) {
        sol2g += phi2[i] * sol2[i];
      }

      // *** phi_i loop ***
      for (unsigned i = 0; i < nDofs1; i++) {
        sol->_Sol[solw1]->add (dof1[i], phi1[i] * weight2);
        sol->_Sol[solsmu1N]->add (dof1[i], sol2g * phi1[i] * weight2);
      } // end phi_i loop
    } // end gauss point loop

  } //end element loop for each process*/
  sol->_Sol[solw1]->close();
  sol->_Sol[solsmu1N]->close();

  for (unsigned i = msh->_dofOffset[solType1][iproc]; i < msh->_dofOffset[solType1][iproc + 1]; i++) {

    double weight = (*sol->_Sol[solw1]) (i);
    double value = (*sol->_Sol[solsmu1N]) (i);

    double mu[2];
    double normMu = 0.;
    for (unsigned k = 0; k < dim; k++) {
      mu[k] = (*sol->_Sol[solmu[k]]) (i);
      normMu += mu[k] * mu[k];
    }
    normMu = sqrt (normMu);
    for (unsigned k = 0; k < dim; k++) {
      sol->_Sol[solmu[k]]->set (i, value / weight * mu[k] / normMu);
    }
    sol->_Sol[solsmu1N]->set (i, value / weight);
  }

  for (unsigned k = 0; k < dim; k++) {
    sol->_Sol[solmu[k]]->close();
  }

  sol->_Sol[solsmu1N]->close();

}


