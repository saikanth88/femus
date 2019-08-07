#include "FemusInit.hpp"
#include "MultiLevelSolution.hpp"
#include "MultiLevelProblem.hpp"
#include "NonLinearImplicitSystemWithPrimalDualActiveSetMethod.hpp"
#include "NumericVector.hpp"
#include "Assemble_jacobian.hpp"
#include "Assemble_unknown_jacres.hpp"

#include "ElemType.hpp"


#define FACE_FOR_CONTROL             2  //we do control on the right (=2) face
#define AXIS_DIRECTION_CONTROL_SIDE  1 //change this accordingly to the other variable above

#include "../../param.hpp"

using namespace femus;

double InitialValueActFlag(const std::vector < double >& x) {
  return 0.;
}

double InitialValueContReg(const std::vector < double >& x) {
  return ControlDomainFlag_bdry(x);
}

double InitialValueTargReg(const std::vector < double >& x) {
  return ElementTargetFlag(x);
}

double InitialValueState(const std::vector < double >& x) {
  return 0.;
}

double InitialValueAdjoint(const std::vector < double >& x) {
  return 0.;
}

double InitialValueMu(const std::vector < double >& x) {
  return 0.;
}

double InitialValueControl(const std::vector < double >& x) {
  return 0.;
}

bool SetBoundaryCondition(const std::vector < double >& x, const char name[], double& value, const int faceName, const double time) {

  bool dirichlet = true; //dirichlet
  value = 0.;

  if(!strcmp(name,"control")) {
  if (faceName == FACE_FOR_CONTROL) {
  if (x[AXIS_DIRECTION_CONTROL_SIDE] > GAMMA_CONTROL_LOWER - 1.e-5 && x[AXIS_DIRECTION_CONTROL_SIDE] < GAMMA_CONTROL_UPPER + 1.e-5)    
    dirichlet = false;
  }
  }

  if(!strcmp(name,"state")) {  //"state" corresponds to the first block row (u = q)
  if (faceName == FACE_FOR_CONTROL) {
  if (x[AXIS_DIRECTION_CONTROL_SIDE] > GAMMA_CONTROL_LOWER - 1.e-5 && x[AXIS_DIRECTION_CONTROL_SIDE] < GAMMA_CONTROL_UPPER + 1.e-5)    
    dirichlet = false;
  }
      
}

  if(!strcmp(name,"mu")) {
//       value = 0.;
    dirichlet = false;
  }
  
//     if(!strcmp(name,"adjoint")) { 
//     dirichlet = false;
//   }

  
  return dirichlet;
}


void ComputeIntegral(const MultiLevelProblem& ml_prob);

void AssembleOptSys(MultiLevelProblem& ml_prob);


int main(int argc, char** args) {

  // init Petsc-MPI communicator
  FemusInit mpinit(argc, args, MPI_COMM_WORLD);
  
  // ======= Files ========================
  Files files; 
  files.CheckIODirectories();
  files.RedirectCout();

  // ======= Quad Rule ========================
  std::string fe_quad_rule("seventh");

  // define multilevel mesh
  MultiLevelMesh ml_msh;

//   ml_msh.GenerateCoarseBoxMesh(NSUB_X,NSUB_Y,0,0.,1.,0.,1.,0.,0.,QUAD9,fe_quad_rule.c_str());
  
  std::string input_file = "square_parametric.med";
  std::ostringstream mystream; mystream << "./" << DEFAULT_INPUTDIR << "/" << input_file;
  const std::string infile = mystream.str();
  const double Lref = 1.;
  ml_msh.ReadCoarseMesh(infile.c_str(),fe_quad_rule.c_str(),Lref);

  
   //1: bottom  //2: right  //3: top  //4: left
  
 /* "seventh" is the order of accuracy that is used in the gauss integration scheme
      probably in the furure it is not going to be an argument of this function   */
  unsigned numberOfUniformLevels = 6;
  unsigned numberOfSelectiveLevels = 0;
  ml_msh.RefineMesh(numberOfUniformLevels , numberOfUniformLevels + numberOfSelectiveLevels, NULL);
  ml_msh.EraseCoarseLevels(numberOfUniformLevels - 1);
  ml_msh.PrintInfo();

  // define the multilevel solution and attach the ml_msh object to it
  MultiLevelSolution ml_sol(&ml_msh);

  // add variables to ml_sol
  ml_sol.AddSolution("state", LAGRANGE, FIRST);
  ml_sol.AddSolution("control", LAGRANGE, FIRST);
  ml_sol.AddSolution("adjoint", LAGRANGE, FIRST);
  ml_sol.AddSolution("mu", LAGRANGE, FIRST);  
  ml_sol.AddSolution("TargReg",  DISCONTINUOUS_POLYNOMIAL, ZERO); //this variable is not solution of any eqn, it's just a given field
  ml_sol.AddSolution("ContReg",  DISCONTINUOUS_POLYNOMIAL, ZERO); //this variable is not solution of any eqn, it's just a given field
  const unsigned int fake_time_dep_flag = 2;
  const std::string act_set_flag_name = "act_flag";
  ml_sol.AddSolution(act_set_flag_name.c_str(), LAGRANGE, FIRST,fake_time_dep_flag);               //this variable is not solution of any eqn, it's just a given field

  
  ml_sol.Initialize("All");    // initialize all varaibles to zero

  ml_sol.Initialize("state", InitialValueState);
  ml_sol.Initialize("control", InitialValueControl);
  ml_sol.Initialize("adjoint", InitialValueAdjoint);
  ml_sol.Initialize("mu", InitialValueMu);
  ml_sol.Initialize("TargReg", InitialValueTargReg);
  ml_sol.Initialize("ContReg", InitialValueContReg);
  ml_sol.Initialize(act_set_flag_name.c_str(), InitialValueActFlag);

  // attach the boundary condition function and generate boundary data
  ml_sol.AttachSetBoundaryConditionFunction(SetBoundaryCondition);
  ml_sol.GenerateBdc("state");
  ml_sol.GenerateBdc("control");
  ml_sol.GenerateBdc("adjoint");
  ml_sol.GenerateBdc("mu");  //we need add this to make the matrix iterations work...

  // define the multilevel problem attach the ml_sol object to it
  MultiLevelProblem mlProb(&ml_sol);
  
  mlProb.SetQuadratureRuleAllGeomElems(fe_quad_rule);
  mlProb.SetFilesHandler(&files);

 // add system  in mlProb as a Linear Implicit System
  NonLinearImplicitSystemWithPrimalDualActiveSetMethod& system = mlProb.add_system < NonLinearImplicitSystemWithPrimalDualActiveSetMethod > ("LiftRestr");
  
  system.SetActiveSetFlagName(act_set_flag_name);
//   system.SetMaxNumberOfNonLinearIterations(50);

  system.AddSolutionToSystemPDE("state");  
  system.AddSolutionToSystemPDE("control");  
  system.AddSolutionToSystemPDE("adjoint");  
  system.AddSolutionToSystemPDE("mu");  
  
  // attach the assembling function to system
  system.SetAssembleFunction(AssembleOptSys);
  
  ml_sol.SetWriter(VTK);
  ml_sol.GetWriter()->SetDebugOutput(true);

  system.SetDebugNonlinear(true);
  system.SetDebugFunction(ComputeIntegral);  //weird error if I comment this line, I expect nothing to happen but something in the assembly gets screwed up in memory I guess
   
  // initialize and solve the system
  system.init();
  system.MGsolve();
  
 
  // print solutions
  std::vector < std::string > variablesToBePrinted;
  variablesToBePrinted.push_back("all");
  ml_sol.GetWriter()->Write(files.GetOutputPath()/*DEFAULT_OUTPUTDIR*/, "biquadratic", variablesToBePrinted);

  return 0;
}




void AssembleOptSys(MultiLevelProblem& ml_prob) {
    
  //  ml_prob is the global object from/to where get/set all the data

  //  level is the level of the PDE system to be assembled
  //  levelMax is the Maximum level of the MultiLevelProblem
  //  assembleMatrix is a flag that tells if only the residual or also the matrix should be assembled

  //  extract pointers to the several objects that we are going to use

  NonLinearImplicitSystemWithPrimalDualActiveSetMethod* mlPdeSys  = &ml_prob.get_system<NonLinearImplicitSystemWithPrimalDualActiveSetMethod> ("LiftRestr");
  const unsigned level = mlPdeSys->GetLevelToAssemble();
  const bool assembleMatrix = mlPdeSys->GetAssembleMatrix();

  Mesh*                    msh = ml_prob._ml_msh->GetLevel(level);    // pointer to the mesh (level) object
  elem*                     el = msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution*    ml_sol = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution*                sol = ml_prob._ml_sol->GetSolutionLevel(level);    // pointer to the solution (level) object

  LinearEquationSolver* pdeSys = mlPdeSys->_LinSolver[level]; // pointer to the equation (level) object
  SparseMatrix*             KK = pdeSys->_KK;  // pointer to the global stifness matrix object in pdeSys (level)
  NumericVector*           RES = pdeSys->_RES; // pointer to the global residual vector object in pdeSys (level)

  const unsigned  dim = msh->GetDimension(); // get the domain dimension of the problem
  unsigned dim2 = (3 * (dim - 1) + !(dim - 1));        // dim2 is the number of second order partial derivatives (1,3,6 depending on the dimension)
  const unsigned max_size = static_cast< unsigned >(ceil(pow(3, dim)));

  unsigned    iproc = msh->processor_id(); // get the process_id (for parallel computation)

 //*****************integration ********************************** 
  double weight = 0.;
  double weight_bdry = 0.;


 //**************** geometry *********************************** 
  unsigned solType_coords = 0; //we do linear FE this time // get the finite element type for "x", it is always 2 (LAGRANGE QUADRATIC)
  vector < vector < double > > x(dim);
  vector < vector < double> >  coords_at_dofs_bdry(dim);
  for (unsigned i = 0; i < dim; i++) {
         x[i].reserve(max_size);
	 coords_at_dofs_bdry[i].reserve(max_size);
  }
  
  vector < double > coord_at_qp_bdry(dim);
  
  vector <double> phi_coords;
  vector <double> phi_coords_x;
  vector <double> phi_coords_xx; 

  phi_coords.reserve(max_size);
  phi_coords_x.reserve(max_size * dim);
  phi_coords_xx.reserve(max_size * dim2);
  
  vector <double> phi_coords_bdry;  
  vector <double> phi_coords_x_bdry; 

  phi_coords_bdry.reserve(max_size);
  phi_coords_x_bdry.reserve(max_size * dim);
 //*************************************************** 

 //********************* state *********************** 
 //*************************************************** 
  vector <double> phi_u;  // local test function
  vector <double> phi_u_x; // local test function first order partial derivatives
  vector <double> phi_u_xx; // local test function second order partial derivatives

  phi_u.reserve(max_size);
  phi_u_x.reserve(max_size * dim);
  phi_u_xx.reserve(max_size * dim2);
  
  
  //boundary state shape functions
  vector <double> phi_u_bdry;  
  vector <double> phi_u_x_bdry; 

  phi_u_bdry.reserve(max_size);
  phi_u_x_bdry.reserve(max_size * dim);
  
 //***************************************************  
 //***************************************************  

  
 //********************** adjoint ********************
 //*************************************************** 
  vector <double> phi_adj;  // local test function
  vector <double> phi_adj_x; // local test function first order partial derivatives
  vector <double> phi_adj_xx; // local test function second order partial derivatives

  phi_adj.reserve(max_size);
  phi_adj_x.reserve(max_size * dim);
  phi_adj_xx.reserve(max_size * dim2);
 

  //boundary adjoint shape functions  
  vector <double> phi_adj_bdry;  
  vector <double> phi_adj_x_bdry; 

  phi_adj_bdry.reserve(max_size);
  phi_adj_x_bdry.reserve(max_size * dim);
  
  vector <double> phi_adj_vol_at_bdry;  // local test function
  vector <double> phi_adj_x_vol_at_bdry; // local test function first order partial derivatives
  phi_adj_vol_at_bdry.reserve(max_size);
  phi_adj_x_vol_at_bdry.reserve(max_size * dim);
  vector <double> sol_adj_x_vol_at_bdry_gss(dim);
 //*************************************************** 
 //*************************************************** 

  
 //********************* bdry cont *******************
 //*************************************************** 
  vector <double> phi_ctrl_bdry;  
  vector <double> phi_ctrl_x_bdry; 

  phi_ctrl_bdry.reserve(max_size);
  phi_ctrl_x_bdry.reserve(max_size * dim);
 //*************************************************** 

  //************** act flag **************************** 
  std::string act_flag_name = "act_flag";
  unsigned int solIndex_act_flag = ml_sol->GetIndex(act_flag_name.c_str());
  unsigned int solFEType_act_flag = ml_sol->GetSolutionType(solIndex_act_flag); 
     if(sol->GetSolutionTimeOrder(solIndex_act_flag) == 2) {
       *(sol->_SolOld[solIndex_act_flag]) = *(sol->_Sol[solIndex_act_flag]);
     }
  
  
  
  //********* variables for ineq constraints *****************
  const int ineq_flag = INEQ_FLAG;
  const double c_compl = C_COMPL;
  vector < double/*int*/ >  sol_actflag;   sol_actflag.reserve(max_size); //flag for active set
  vector < double >  ctrl_lower;   ctrl_lower.reserve(max_size);
  vector < double >  ctrl_upper;   ctrl_upper.reserve(max_size);
  //***************************************************  
  
  
//***************************************************
//********* WHOLE SET OF VARIABLES ******************
    const unsigned int n_unknowns = mlPdeSys->GetSolPdeIndex().size();

    enum Sol_pos {pos_state=0, pos_ctrl, pos_adj, pos_mu}; //these are known at compile-time 
                    ///@todo these are the positions in the MlSol object or in the Matrix? I'd say the matrix, but we have to check where we use it...

    assert(pos_state   == mlPdeSys->GetSolPdeIndex("state"));
    assert(pos_ctrl    == mlPdeSys->GetSolPdeIndex("control"));
    assert(pos_adj     == mlPdeSys->GetSolPdeIndex("adjoint"));
    assert(pos_mu      == mlPdeSys->GetSolPdeIndex("mu"));


    vector < std::string > Solname(n_unknowns);
    Solname[0] = "state";
    Solname[1] = "control";
    Solname[2] = "adjoint";
    Solname[3] = "mu";

    vector < unsigned > SolPdeIndex(n_unknowns);
    vector < unsigned > SolIndex(n_unknowns);
    vector < unsigned > SolFEType(n_unknowns);


    for(unsigned ivar=0; ivar < n_unknowns; ivar++) {
        SolPdeIndex[ivar] = mlPdeSys->GetSolPdeIndex(Solname[ivar].c_str());
        SolIndex[ivar]    = ml_sol->GetIndex        (Solname[ivar].c_str());
        SolFEType[ivar]   = ml_sol->GetSolutionType(SolIndex[ivar]);
    }

    vector < unsigned > Sol_n_el_dofs(n_unknowns);

//***************************************************
    //----------- quantities (at dof objects) ------------------------------
    std::vector< int >       L2G_dofmap_AllVars;
    L2G_dofmap_AllVars.reserve( n_unknowns * max_size );
    vector < vector < int > >     L2G_dofmap(n_unknowns);
    for(int i = 0; i < n_unknowns; i++) {
        L2G_dofmap[i].reserve(max_size);
    }
    
    vector < vector < double > >  sol_eldofs(n_unknowns);
    for(int k = 0; k < n_unknowns; k++) {        sol_eldofs[k].reserve(max_size);    }


 //*************************************************** 
  const int solType_max = 2;  //biquadratic
 
  std::vector< double > Res;   Res.reserve( n_unknowns*max_size);
  std::vector < double > Jac;  Jac.reserve( n_unknowns*max_size * n_unknowns*max_size);
 //*************************************************** 

 
 //********************* DATA ************************ 
  double u_des = DesiredTarget();
  double alpha = ALPHA_CTRL_BDRY;
  double beta  = BETA_CTRL_BDRY;
  double penalty_outside_control_boundary = 1.e50;       // penalty for zero control outside Gamma_c
  double penalty_strong_bdry = 1.e20;  // penalty for boundary equation on Gamma_c
  double penalty_ctrl = 1.e10;         //penalty for u=q
 //*************************************************** 
  
  
  if (assembleMatrix)  KK->zero();

    
  // element loop: each process loops only on the elements that owns
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    short unsigned ielGeom = msh->GetElementType(iel);    // element geometry type

 //********************* GEOMETRY *********************
    unsigned nDofx = msh->GetElementDofNumber(iel, solType_coords);    // number of coordinate element dofs
    for (int i = 0; i < dim; i++)  x[i].resize(nDofx);
    
    for (unsigned i = 0; i < nDofx; i++) {
      unsigned xDof  = msh->GetSolutionDof(i, iel, solType_coords);    // global to global mapping between coordinates node and coordinate dof // via local to global solution node

      for (unsigned jdim = 0; jdim < dim; jdim++) {
        x[jdim][i] = (*msh->_topology->_Sol[jdim])(xDof);      // global extraction and local storage for the element coordinates
      }
    }
    

 //*******************elem average point**************
    vector < double > elem_center(dim);   
    for (unsigned j = 0; j < dim; j++) {  elem_center[j] = 0.;  }
  for (unsigned j = 0; j < dim; j++) {  
      for (unsigned i = 0; i < nDofx; i++) {
         elem_center[j] += x[j][i];
       }
    }
    
   for (unsigned j = 0; j < dim; j++) { elem_center[j] = elem_center[j]/nDofx; }
 //*************************************************** 
  
  
 //************* set target domain flag **************
   int target_flag = 0;
   target_flag = ElementTargetFlag(elem_center);
 //*************************************************** 
   
   
 //************ set control flag *********************
  int control_el_flag = 0;
        control_el_flag = ControlDomainFlag_bdry(elem_center);
  std::vector<int> control_node_flag(nDofx,0);
//   if (control_el_flag == 0) std::fill(control_node_flag.begin(), control_node_flag.end(), 0);
 //*************************************************** 
    
    
        //all vars###################################################################
        for (unsigned  k = 0; k < n_unknowns; k++) {
            unsigned  ndofs_unk = msh->GetElementDofNumber(iel, SolFEType[k]);
            Sol_n_el_dofs[k] = ndofs_unk;
            sol_eldofs[k].resize(ndofs_unk);
            L2G_dofmap[k].resize(ndofs_unk);
            for (unsigned i = 0; i < ndofs_unk; i++) {
                unsigned solDof = msh->GetSolutionDof(i, iel, SolFEType[k]);
                sol_eldofs[k][i] = (*sol->_Sol[SolIndex[k]])(solDof);
                L2G_dofmap[k][i] = pdeSys->GetSystemDof(SolIndex[k], SolPdeIndex[k], i, iel);
            }
        }
        //all vars###################################################################
 
    unsigned int nDof_max          = ElementJacRes<double>::compute_max_n_dofs(Sol_n_el_dofs);
    
    unsigned int sum_Sol_n_el_dofs = ElementJacRes<double>::compute_sum_n_dofs(Sol_n_el_dofs);

    Res.resize(sum_Sol_n_el_dofs);
    std::fill(Res.begin(), Res.end(), 0.);

    Jac.resize(sum_Sol_n_el_dofs * sum_Sol_n_el_dofs);
    std::fill(Jac.begin(), Jac.end(), 0.);
    
    L2G_dofmap_AllVars.resize(0);
      for (unsigned  k = 0; k < n_unknowns; k++)     L2G_dofmap_AllVars.insert(L2G_dofmap_AllVars.end(),L2G_dofmap[k].begin(),L2G_dofmap[k].end());
 //***************************************************

    
 //===================================================   

	// Perform face loop over elements that contain some control face
	if (control_el_flag == 1) {
	  
	  double tau=0.;
	  vector<double> normal(dim,0);
// 	  double normal_fixed[3] = {0.,1.,0};
	       
	  // loop on faces of the current element

	  for(unsigned jface=0; jface < msh->GetElementFaceNumber(iel); jface++) {
          
//========= compute coordinates of boundary nodes on each element ========================================== 
		unsigned nve_bdry = msh->GetElementFaceDofNumber(iel,jface,solType_coords);
	        for (unsigned idim = 0; idim < dim; idim++) {  coords_at_dofs_bdry[idim].resize(nve_bdry); }
		const unsigned felt_bdry = msh->GetElementFaceType(iel, jface);    
		for(unsigned i_bdry=0; i_bdry < nve_bdry; i_bdry++) {
		  unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i_bdry);
                  unsigned iDof = msh->GetSolutionDof(i_vol, iel, solType_coords);
		  for(unsigned idim=0; idim<dim; idim++) {
		      coords_at_dofs_bdry[idim][i_bdry]=(*msh->_topology->_Sol[idim])(iDof);
		  }
		}
//==========================================================================================================   
 
          std::vector < double > elem_center_bdry(dim, 0.);  elem_center_bdry =  face_elem_center(coords_at_dofs_bdry);

	    // look for boundary faces
            const int bdry_index = el->GetFaceElementIndex(iel,jface);
            
	    if( bdry_index < 0) {
	      unsigned int face_in_rectangle_domain = -( msh->el->GetFaceElementIndex(iel,jface)+1);
		
// 	      if( !ml_sol->_SetBoundaryConditionFunction(xx,"U",tau,face,0.) && tau!=0.){
	      if(  face_in_rectangle_domain == FACE_FOR_CONTROL) { //control face
              
 //=================================================== 
		//we use the dirichlet flag to say: if dirichlet = true, we set 1 on the diagonal. if dirichlet = false, we put the boundary equation
	      bool  dir_bool = ml_sol->GetBdcFunction()(elem_center_bdry,Solname[pos_ctrl].c_str(),tau,face_in_rectangle_domain,0.);

 //=================================================== 

		


        
  update_active_set_flag_for_current_nonlinear_iteration_bdry
   (msh, sol, iel, jface, solType_coords, coords_at_dofs_bdry, sol_eldofs, Sol_n_el_dofs, 
    pos_mu, pos_ctrl, c_compl, ctrl_lower, ctrl_upper, sol_actflag, solFEType_act_flag, solIndex_act_flag);
 
 // ===================================================
 //node-based insertion on the boundary ===============
 // ===================================================
    
 //============= delta_mu row ===============================
      std::vector<double> Res_mu (Sol_n_el_dofs[pos_mu]); std::fill(Res_mu.begin(),Res_mu.end(), 0.);
      
      for (int i_bdry = 0; i_bdry < sol_actflag.size(); i_bdry++)  {
	    unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i_bdry);
//     for (unsigned i = 0; i < sol_actflag.size(); i++) {
      if (sol_actflag[i_bdry] == 0){  //inactive
         Res_mu [i_vol] = - ineq_flag * ( 1. * sol_eldofs[pos_mu][i_vol] - 0. ); 
// 	 Res_mu [i] = Res[nDof_u + nDof_ctrl + nDof_adj + i]; 
      }
      else if (sol_actflag[i_bdry] == 1){  //active_a 
	 Res_mu [i_vol] = - ineq_flag * ( c_compl *  sol_eldofs[pos_ctrl][i_vol] - c_compl * ctrl_lower[i_bdry]);
      }
      else if (sol_actflag[i_bdry] == 2){  //active_b 
	Res_mu [i_vol]  =  - ineq_flag * ( c_compl *  sol_eldofs[pos_ctrl][i_vol] - c_compl * ctrl_upper[i_bdry]);
      }
    }

    
    RES->insert(Res_mu,  L2G_dofmap[pos_mu]);    
 //============= delta_mu row - end ===============================
    
 //============= delta_mu-delta_ctrl row ===============================
 //auxiliary volume vector for act flag
 unsigned nDof_actflag_vol  = msh->GetElementDofNumber(iel, solFEType_act_flag);
 std::vector<double> sol_actflag_vol(nDof_actflag_vol); 


 for (unsigned i_bdry = 0; i_bdry < sol_actflag.size(); i_bdry++) if (sol_actflag[i_bdry] != 0 ) sol_actflag[i_bdry] = ineq_flag * c_compl;    
 
 std::fill(sol_actflag_vol.begin(), sol_actflag_vol.end(), 0.);
    for (int i_bdry = 0; i_bdry < sol_actflag.size(); i_bdry++)  {
       unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i_bdry);
       sol_actflag_vol[i_vol] = sol_actflag[i_bdry];
    }
 
 KK->matrix_set_off_diagonal_values_blocked(L2G_dofmap[pos_mu], L2G_dofmap[pos_ctrl], sol_actflag_vol);
 //============= delta_mu-delta_ctrl row - end ===============================

 //============= delta_mu-delta_mu row ===============================
  for (unsigned i_bdry = 0; i_bdry < sol_actflag.size(); i_bdry++) sol_actflag[i_bdry] =  ineq_flag * (1 - sol_actflag[i_bdry]/c_compl)  + (1-ineq_flag) * 1.;  //can do better to avoid division, maybe use modulo operator 

 std::fill(sol_actflag_vol.begin(), sol_actflag_vol.end(), 0.);
    for (int i_bdry = 0; i_bdry < sol_actflag.size(); i_bdry++)  {
       unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i_bdry);
       sol_actflag_vol[i_vol] = sol_actflag[i_bdry];
    }
  
  KK->matrix_set_off_diagonal_values_blocked(L2G_dofmap[pos_mu], L2G_dofmap[pos_mu], sol_actflag_vol );
 //============= delta_mu-delta_mu row - end ===============================
    
 // =========================================================
 //node-based insertion on the boundary - end ===============
 // =========================================================
    
 
//========= initialize gauss quantities on the boundary ============================================
                double sol_ctrl_bdry_gss = 0.;
                double sol_adj_bdry_gss = 0.;
                std::vector<double> sol_ctrl_x_bdry_gss(dim);   std::fill(sol_ctrl_x_bdry_gss.begin(), sol_ctrl_x_bdry_gss.end(), 0.);

//========= initialize gauss quantities on the boundary ============================================
		
        const unsigned n_gauss_bdry = ml_prob.GetQuadratureRule(felt_bdry).GetGaussPointsNumber();
        
        //show the coordinate of the current ig_bdry point    
    const double* pt_one_dim[1] = {msh->_finiteElement[ielGeom][ SolFEType[pos_ctrl] ]->GetGaussRule_bdry()->GetGaussWeightsPointer() + 1*n_gauss_bdry };
    
		for(unsigned ig_bdry=0; ig_bdry < n_gauss_bdry; ig_bdry++) {
    
      double xi_one_dim[1];
      for (unsigned j = 0; j < 1; j++) {
        xi_one_dim[j] = *pt_one_dim[j];
        pt_one_dim[j]++;
      }

std::cout << "Outside ig = " << ig_bdry << " ";
      for (unsigned d = 0; d < 1; d++) std::cout << xi_one_dim[d] << " ";
            
		  msh->_finiteElement[felt_bdry][SolFEType[pos_state]]->JacobianSur(coords_at_dofs_bdry,ig_bdry,weight_bdry,phi_ctrl_bdry,phi_ctrl_x_bdry,normal);
          msh->_finiteElement[felt_bdry][SolFEType[pos_ctrl]]->JacobianSur(coords_at_dofs_bdry,ig_bdry,weight_bdry,phi_u_bdry,phi_u_x_bdry,normal);
		  msh->_finiteElement[felt_bdry][SolFEType[pos_adj]]->JacobianSur(coords_at_dofs_bdry,ig_bdry,weight_bdry,phi_adj_bdry,phi_adj_x_bdry,normal);
		  msh->_finiteElement[felt_bdry][solType_coords]->JacobianSur(coords_at_dofs_bdry,ig_bdry,weight_bdry,phi_coords_bdry,phi_coords_x_bdry,normal);
      
 //========= fill gauss value xyz ==================   
   // it is the JacobianSur function that defines the mapping between real quadrature points and reference quadrature points 
   // and that is the result of how the element is oriented (how the nodes are listed)
   // the fact is that you don't know where this boundary gauss point is located with respect to the reference VOLUME...
   //it could be on xi = -1, xi=1, eta=-1, eta=1...
   //what I know is that all that matters eventually is to find the corresponding REFERENCE position, because that's where I will evaluate my derivatives at the boundary,
   // to compute the normal derivative and so on
   //so, I propose once and for all to make a JACOBIAN FUNCTION that depends on the REAL coordinate, and yields the CANONICAL ONE.
    
   // The alternative to this approach, which is the most general one, is to do like I did with the cosines and so on to switch between X and Y axis,
   // but as soon as you'll have an inclined boundary you'll be stuck. So let's go general
      
   //The problem with the general approach is that the Gauss evaluation is done INSIDE this Gauss loop, instead of being done once and for all OUTSIDE
   //One should build a map that says: 
   // "if this is the face(top/bottome/left/right) in the reference element AND if the real face is oriented concordantly/discordantly with respect to the reference face, then    
   // use this point or the other point..." Not very convenient
      
   //Another problem with the general approach is that it is the INVERSION of the JACOBIAN MAPPING, and that can only be done when the mapping is a LINEAR FUNCTION...
      
   std::fill(coord_at_qp_bdry.begin(), coord_at_qp_bdry.end(), 0.);
    for (unsigned  d = 0; d < dim; d++) {
        	for (unsigned i = 0; i < coords_at_dofs_bdry[d].size(); i++) {
               coord_at_qp_bdry[d] += coords_at_dofs_bdry[d][i] * phi_coords_bdry[i];
            }
std::cout <<  " qp_" << d << " " << coord_at_qp_bdry[d];
    }
    
  //========= fill gauss value xyz ==================   
  
          if (ielGeom != QUAD) { std::cout << "VolumeShapeAtBoundary not implemented" << std::endl; abort(); } 
		  msh->_finiteElement[ielGeom][SolFEType[pos_adj]]->VolumeShapeAtBoundary(x,coords_at_dofs_bdry,jface,ig_bdry,phi_adj_vol_at_bdry,phi_adj_x_vol_at_bdry);

//           std::cout << "elem " << iel << " ig_bdry " << ig_bdry;
// 		      for (int iv = 0; iv < nDof_adj; iv++)  {
//                   for (int d = 0; d < dim; d++) {
//                        std::cout << " " <<   phi_adj_x_vol_at_bdry[iv * dim + d];
//                 }
//               }

      
      
//========== temporary soln for surface gradient on a face parallel to the axes ===================
          const unsigned int axis_direction_control_side = AXIS_DIRECTION_CONTROL_SIDE;
		  double dx_dcurv_abscissa = 0.;
		 const elem_type_1D * myeltype = static_cast<const elem_type_1D*>(msh->_finiteElement[felt_bdry][SolFEType[pos_ctrl]]);
		 double * myptr = myptr = myeltype->GetDPhiDXi(ig_bdry);
		      for (int inode = 0; inode < nve_bdry/*_nc*/; inode++) dx_dcurv_abscissa += myptr[inode] * coords_at_dofs_bdry[axis_direction_control_side][inode];
  
		      for (int inode = 0; inode < nve_bdry/*_nc*/; inode++) {
                            for (int d = 0; d < dim; d++) {
                              if (d == axis_direction_control_side ) phi_ctrl_x_bdry[inode + d*nve_bdry/*_nc*/] = myptr[inode]* (1./ dx_dcurv_abscissa);
                              else  phi_ctrl_x_bdry[inode + d*nve_bdry/*_nc*/] = 0.;
                         }
                     }
//========== temporary soln for surface gradient on a face parallel to the axes ===================
		  
//========== compute gauss quantities on the boundary ===============================================
		  sol_ctrl_bdry_gss = 0.;
		  sol_adj_bdry_gss = 0.;
                  std::fill(sol_ctrl_x_bdry_gss.begin(), sol_ctrl_x_bdry_gss.end(), 0.);
		      for (int i_bdry = 0; i_bdry < nve_bdry/*_nc*/; i_bdry++)  {
		    unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i_bdry);
			
			sol_adj_bdry_gss  +=  sol_eldofs[pos_adj][i_vol] * phi_adj_bdry[i_bdry];
			sol_ctrl_bdry_gss +=  sol_eldofs[pos_ctrl][i_vol] * phi_ctrl_bdry[i_bdry];
                            for (int d = 0; d < dim; d++) {
			      sol_ctrl_x_bdry_gss[d] += sol_eldofs[pos_ctrl][i_vol] * phi_ctrl_x_bdry[i_bdry + d*nve_bdry];
			    }
		      }
		      
//=============== grad dot n for residual ========================================= 
//     compute gauss quantities on the boundary through VOLUME interpolation
           std::fill(sol_adj_x_vol_at_bdry_gss.begin(), sol_adj_x_vol_at_bdry_gss.end(), 0.);
		      for (int iv = 0; iv < Sol_n_el_dofs[pos_adj]; iv++)  {
			
                            for (int d = 0; d < dim; d++) {
//    std::cout << " ivol " << iv << std::endl;
//    std::cout << " adj dofs " << sol_adj[iv] << std::endl;
			      sol_adj_x_vol_at_bdry_gss[d] += sol_eldofs[pos_adj][iv] * phi_adj_x_vol_at_bdry[iv * dim + d];//notice that the convention of the orders x y z is different from vol to bdry
			    }
		      }  
		      
    double grad_adj_dot_n_res = 0.;
        for(unsigned d=0; d<dim; d++) {
	  grad_adj_dot_n_res += sol_adj_x_vol_at_bdry_gss[d] * normal[d];  
	}
//=============== grad dot n  for residual =========================================       

//========== compute gauss quantities on the boundary ================================================

		  // *** phi_i loop ***
		  for(unsigned i_bdry=0; i_bdry < nve_bdry; i_bdry++) {
		    unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i_bdry);

                 double lap_rhs_dctrl_ctrl_bdry_gss_i = 0.;
                 for (unsigned d = 0; d < dim; d++) {
                       if ( i_vol < Sol_n_el_dofs[pos_ctrl] )  lap_rhs_dctrl_ctrl_bdry_gss_i +=  phi_ctrl_x_bdry[i_bdry + d*nve_bdry] * sol_ctrl_x_bdry_gss[d];
                 }
                 
//=============== construct control node flag field on the go  =========================================    
	      /* (control_node_flag[i])       picks nodes on \Gamma_c
	         (1 - control_node_flag[i])   picks nodes on \Omega \setminus \Gamma_c
	       */
	      if (dir_bool == false) { 
// 		std::cout << " found boundary control nodes ==== " << std::endl;
			for(unsigned k=0; k<control_node_flag.size(); k++) {
				  control_node_flag[i_vol] = 1;
			}
              }
//=============== construct control node flag field on the go  =========================================    

		 
//============ Bdry Residuals ==================	
                Res[ assemble_jacobian<double,double>::res_row_index(Sol_n_el_dofs,pos_state,i_vol) ] +=  - control_node_flag[i_vol] * penalty_ctrl * (   sol_eldofs[pos_state][i_vol] - sol_eldofs[pos_ctrl][i_vol] )
                    - control_node_flag[i_vol] *  weight_bdry * (grad_adj_dot_n_res * phi_u_bdry[i_bdry]);   // u = q
//                                      Res[ (nDof_u + i_vol) ]               +=  - control_node_flag[i_vol] * penalty_ctrl * (   sol_ctrl[i_vol] - sol_adj[i_vol] );   // q = lambda for testing
		
                Res[ assemble_jacobian<double,double>::res_row_index(Sol_n_el_dofs,pos_ctrl,i_vol) ]  +=  - control_node_flag[i_vol] *  weight_bdry *
                                                                                (    alpha * phi_ctrl_bdry[i_bdry] * sol_ctrl_bdry_gss
							                           +  beta * lap_rhs_dctrl_ctrl_bdry_gss_i 
							                           - grad_adj_dot_n_res * phi_ctrl_bdry[i_bdry]
// 							                           -         phi_ctrl_bdry[i_bdry]*sol_adj_bdry_gss // for Neumann control
							                         );  //boundary optimality condition
                Res[ assemble_jacobian<double,double>::res_row_index(Sol_n_el_dofs,pos_adj,i_vol) ]  += 0.; 
//============ Bdry Residuals ==================    
		    
		    for(unsigned j_bdry=0; j_bdry < nve_bdry; j_bdry ++) {
		         unsigned int j_vol = msh->GetLocalFaceVertexIndex(iel, jface, j_bdry);

//============ Bdry Jacobians ==================	
//============ Bdry Jacobians ==================	


// FIRST BLOCK ROW
//============ u = q ===========================	    
                 
if ( i_vol == j_vol )  {
		Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_state, pos_state, i_vol, j_vol) ] += penalty_ctrl * ( control_node_flag[i_vol]);
		Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_state, pos_ctrl, i_vol, j_vol) ]  += penalty_ctrl * ( control_node_flag[i_vol]) * (-1.);
		}
//============ u = q ===========================

		    

// SECOND BLOCK ROW
//=========== boundary control eqn =============	    

//========block delta_control / control ========
              double  lap_mat_dctrl_ctrl_bdry_gss = 0.;
		      for (unsigned d = 0; d < dim; d++) {  lap_mat_dctrl_ctrl_bdry_gss += phi_ctrl_x_bdry[i_bdry + d*nve_bdry] * phi_ctrl_x_bdry[j_bdry + d*nve_bdry];    }

          
              Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_ctrl, pos_ctrl, i_vol, j_vol) ] 
			+=  control_node_flag[i_vol] *  weight_bdry * (alpha * phi_ctrl_bdry[i_bdry] * phi_ctrl_bdry[j_bdry] 
			                                              + beta *  lap_mat_dctrl_ctrl_bdry_gss);

/*//==========block delta_control/adjoint ========
		   if ( i_vol < nDof_ctrl    && j_vol < nDof_adj)   
		     Jac[ 
			(nDof_u + i_vol) * sum_Sol_n_el_dofs  +
		        (nDof_u + nDof_ctrl + j_vol)             ]  += control_node_flag[i_vol] * (-1) *
		        (
			  weight_bdry * grad_adj_dot_n_mat * phi_ctrl_bdry[i_bdry]
// 			  weight_bdry * phi_adj_bdry[j_bdry] * phi_ctrl_bdry[i_bdry]  // for neumann boundary condition
			  
			);    */  
		    

//============ boundary control eqn ============	    

//============ q = lambda for testing ==========	    

//      // block delta_control / control ========
//    
//    	      if ( i_vol < nDof_ctrl && j_vol < nDof_ctrl && i_vol == j_vol) {
//               Jac[  
// 		    (nDof_u + i_vol) * sum_Sol_n_el_dofs +
// 	            (nDof_u + j_vol) ] 
// 			+=   control_node_flag[i_vol] * penalty_strong_bdry;
// 	      }
//      // block delta_control/adjoint ========
// 		   if ( i_vol < nDof_ctrl    && j_vol < nDof_adj && i_vol == j_vol)   
// 		     Jac[ 
// 			(nDof_u + i_vol) * sum_Sol_n_el_dofs  +
// 		        (nDof_u + nDof_ctrl + j_vol)             ]  += - control_node_flag[i_vol] * penalty_strong_bdry; //weight_bdry * phi_ctrl_bdry[i_bdry] * phi_adj_bdry[j_bdry];      
// 		    
//============ q = lambda for testing ==========	    
		   
//============ End Bdry Jacobians ==================	
//============ End Bdry Jacobians ==================	
				
	      }  //end j loop
	      
//===================loop over j in the VOLUME (while i is in the boundary)	      
	for(unsigned j=0; j < nDof_max; j ++) {
		      
  //=============== grad dot n  =========================================    
    double grad_adj_dot_n_mat = 0.;
        for(unsigned d=0; d<dim; d++) {
	  grad_adj_dot_n_mat += phi_adj_x_vol_at_bdry[j * dim + d]*normal[d];  //notice that the convention of the orders x y z is different from vol to bdry
	}
//=============== grad dot n  =========================================    

  //std::cout << " gradadjdotn " << grad_adj_dot_n_mat << std::endl;
  
		      
//==========block delta_control/adjoint ========
		     Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_ctrl, pos_adj, i_vol, j) ]  += 
		     control_node_flag[i_vol] * (-1) * weight_bdry * grad_adj_dot_n_mat * phi_ctrl_bdry[i_bdry];    		      

//==========block delta_state/adjoint ========
		     Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_state, pos_adj, i_vol, j) ] += 
		     control_node_flag[i_vol] * (1) * weight_bdry * grad_adj_dot_n_mat * phi_u_bdry[i_bdry];  
		      
		    }   //end loop i_bdry // j_vol
	      
	      

//========= debugging ==========================    
//   std::cout << "====== phi values on boundary for gauss point " << ig_bdry << std::endl;
//   
//      for(unsigned i=0; i < nve_bdry; i ++) {
//      std::cout << phi_ctrl_bdry[i] << " ";
//      }
//    std::cout << std::endl;
 
//   std::cout << "====== phi derivatives on boundary for gauss point " << ig_bdry << std::endl;
//   
//   for (unsigned d = 0; d < dim; d++) {
//      for(unsigned i_bdry=0; i_bdry < nve_bdry; i_bdry ++) {
//      std::cout << phi_ctrl_x_bdry[i_bdry + d*nve_bdry] << " ";
//      }
//   }
//========== debugging ==========================    

		  }  //end i loop
		}  //end ig_bdry loop
	      }    //end if control face
	      
	    }  //end if boundary faces
	  }    //end loop over faces
	  
	} //end if control element flag
	

//========= gauss value quantities on the volume ==============  
	double sol_u_gss = 0.;
	double sol_adj_gss = 0.;
	std::vector<double> sol_u_x_gss(dim);     std::fill(sol_u_x_gss.begin(), sol_u_x_gss.end(), 0.);
	std::vector<double> sol_adj_x_gss(dim);   std::fill(sol_adj_x_gss.begin(), sol_adj_x_gss.end(), 0.);
//=============================================== 
 
 
      // *** Gauss point loop ***
      for (unsigned ig = 0; ig < ml_prob.GetQuadratureRule(ielGeom).GetGaussPointsNumber(); ig++) {
	
        // *** get gauss point weight, test function and test function partial derivatives ***
	msh->_finiteElement[ielGeom][SolFEType[pos_state]]  ->Jacobian(x, ig, weight, phi_u, phi_u_x, phi_u_xx);
    msh->_finiteElement[ielGeom][SolFEType[pos_adj]]->Jacobian(x, ig, weight, phi_adj, phi_adj_x, phi_adj_xx);
    msh->_finiteElement[ielGeom][solType_coords]->Jacobian(x, ig, weight, phi_coords, phi_coords_x, phi_coords_xx);
          
	sol_u_gss = 0.;
	sol_adj_gss = 0.;
	std::fill(sol_u_x_gss.begin(), sol_u_x_gss.end(), 0.);
	std::fill(sol_adj_x_gss.begin(), sol_adj_x_gss.end(), 0.);
	
	for (unsigned i = 0; i < Sol_n_el_dofs[pos_state]; i++) {
	                                                sol_u_gss      += sol_eldofs[pos_state][i] * phi_u[i];
                   for (unsigned d = 0; d < dim; d++)   sol_u_x_gss[d] += sol_eldofs[pos_state][i] * phi_u_x[i * dim + d];
          }
	
	for (unsigned i = 0; i < Sol_n_el_dofs[pos_adj]; i++) {
	                                                sol_adj_gss      += sol_eldofs[pos_adj][i] * phi_adj[i];
                   for (unsigned d = 0; d < dim; d++)   sol_adj_x_gss[d] += sol_eldofs[pos_adj][i] * phi_adj_x[i * dim + d];
        }

//==========FILLING WITH THE EQUATIONS ===========
	// *** phi_i loop ***
        for (unsigned i = 0; i < nDof_max; i++) {
	  
              double laplace_rhs_du_adj_i = 0.;
              double laplace_rhs_dadj_u_i = 0.;
              for (unsigned kdim = 0; kdim < dim; kdim++) {
              if ( i < Sol_n_el_dofs[pos_state] )      laplace_rhs_du_adj_i +=  phi_u_x   [i * dim + kdim] * sol_adj_x_gss[kdim];
              if ( i < Sol_n_el_dofs[pos_adj] )    laplace_rhs_dadj_u_i +=  phi_adj_x [i * dim + kdim] * sol_u_x_gss[kdim];
	      }
	      
//============ Volume residuals ==================	    
          Res[ assemble_jacobian<double,double>::res_row_index(Sol_n_el_dofs,pos_state,i) ] += - weight * ( target_flag * phi_u[i] * ( sol_u_gss - u_des)  - laplace_rhs_du_adj_i 
          /**/- phi_u[i] * sol_adj_gss /**/); 
          Res[ assemble_jacobian<double,double>::res_row_index(Sol_n_el_dofs,pos_ctrl,i) ]  += - penalty_outside_control_boundary * ( (1 - control_node_flag[i]) * (  sol_eldofs[pos_ctrl][i] - 0.)  );
          Res[ assemble_jacobian<double,double>::res_row_index(Sol_n_el_dofs,pos_adj,i) ]   += - weight * ((-1) * laplace_rhs_dadj_u_i /**/- phi_adj[i] * sol_u_gss /**/);
          Res[ assemble_jacobian<double,double>::res_row_index(Sol_n_el_dofs,pos_mu,i) ]    += - penalty_outside_control_boundary * ( (1 - control_node_flag[i]) * (  sol_eldofs[pos_mu][i] - 0.)  );
//============  Volume Residuals ==================	    
	      
	      
          if (assembleMatrix) {
	    
            // *** phi_j loop ***
            for (unsigned j = 0; j < nDof_max; j++) {
                
              double laplace_mat_dadj_u = 0.;
              double laplace_mat_du_adj = 0.;

              for (unsigned kdim = 0; kdim < dim; kdim++) {
              if ( i < Sol_n_el_dofs[pos_adj] && j < Sol_n_el_dofs[pos_state] )     laplace_mat_dadj_u        +=  (phi_adj_x [i * dim + kdim] * phi_u_x   [j * dim + kdim]);
              if ( i < Sol_n_el_dofs[pos_state]   && j < Sol_n_el_dofs[pos_adj] )   laplace_mat_du_adj        +=  (phi_u_x   [i * dim + kdim] * phi_adj_x [j * dim + kdim]);
		
	      }

              //============ delta_state row ============================
              // BLOCK delta_state / state
		Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_state, pos_state, i, j) ]  += weight  * target_flag *  phi_u[i] * phi_u[j];   
              //BLOCK delta_state / adjoint
		Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_state, pos_adj, i, j) ]  += weight * (-1) * laplace_mat_du_adj - weight * phi_u[i] * phi_adj[j];
	      
	      
              //=========== delta_control row ===========================
              //enforce control zero outside the control boundary
	      if ( i==j )
		Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_ctrl, pos_ctrl, i, j) ]  += penalty_outside_control_boundary * ( (1 - control_node_flag[i]));    /*weight * phi_adj[i]*phi_adj[j]*/
              
	      //=========== delta_adjoint row ===========================
	      // BLOCK delta_adjoint / state
		Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_adj, pos_state, i, j) ]  += weight * (-1) * laplace_mat_dadj_u - weight * phi_adj[i] * phi_u[j];

              // BLOCK delta_adjoint / adjoint
// 	      if ( i < Sol_n_el_dofs[pos_adj] && j < Sol_n_el_dofs[pos_adj] )
// 		Jac[    
// 		(Sol_n_el_dofs[pos_state] + nDof_ctrl + i) * sum_Sol_n_el_dofs  +
// 		(Sol_n_el_dofs[pos_state] + nDof_ctrl + j)               ]  += 0.;     //weight * phi_adj[i]*phi_adj[j];
	      
	      //============= delta_mu row ===============================
	        if ( i==j )   
		  Jac[ assemble_jacobian<double,double>::jac_row_col_index(Sol_n_el_dofs, sum_Sol_n_el_dofs, pos_mu, pos_mu, i, j) ]  += penalty_outside_control_boundary * ( (1 - control_node_flag[i]));  
          
	         } // end phi_j loop
           } // endif assemble_matrix

        } // end phi_i loop
        
      } // end gauss point loop

      
// 	if (control_el_flag == 1) {
// 	  
//     std::cout << " ========== " << iel << " deltaq/q ================== " << std::endl;      
//          for (unsigned i = 0; i < nDof_max; i++) {
//             for (unsigned j = 0; j < nDof_max; j++) {
// 	      std::cout << " " << std::setfill(' ') << std::setw(10) << Jac[ (Sol_n_el_dofs[pos_state] + i) * sum_Sol_n_el_dofs + (Sol_n_el_dofs[pos_state] + j) ];
// 	     }
// 	      std::cout << std::endl;
// 	   }
// 
//     std::cout << " ========== " << iel << " deltaq/lambda ================== " << std::endl;      
//          for (unsigned i = 0; i < nDof_max; i++) {
//             for (unsigned j = 0; j < nDof_max; j++) {
// 	      std::cout << " " << std::setfill(' ') << std::setw(10) << Jac[ (Sol_n_el_dofs[pos_state] + i) * sum_Sol_n_el_dofs + (Sol_n_el_dofs[pos_state] + nDof_ctrl + j) ];
// 	     }
// 	      std::cout << std::endl;
// 	   }
// 	   
// 	   
// 	}
    
//     std::vector<double> Res_ctrl (nDof_ctrl); std::fill(Res_ctrl.begin(),Res_ctrl.end(), 0.);
//     for (unsigned i = 0; i < sol_ctrl.size(); i++){
//      if ( control_el_flag == 1){
// 	Res[Sol_n_el_dofs[pos_state] + i] = - ( - Res[Sol_n_el_dofs[pos_state] + i] + sol_mu[i] /*- ( 0.4 + sin(M_PI * x[0][i]) * sin(M_PI * x[1][i]) )*/ );
// 	Res_ctrl[i] = Res[Sol_n_el_dofs[pos_state] + i];
//       }
//     }
//     
    //--------------------------------------------------------------------------------------------------------
    // Add the local Matrix/Vector into the global Matrix/Vector

    RES->add_vector_blocked(Res, L2G_dofmap_AllVars);

    if (assembleMatrix) {
      KK->add_matrix_blocked(Jac, L2G_dofmap_AllVars, L2G_dofmap_AllVars);
    }
    
    
    //========== dof-based part, without summation
 


    
 //============= delta_ctrl-delta_mu row ===============================
 KK->matrix_set_off_diagonal_values_blocked(L2G_dofmap[pos_ctrl],  L2G_dofmap[pos_mu], ineq_flag * 1.);
  
    assemble_jacobian<double,double>::print_element_residual(iel, Res, Sol_n_el_dofs, 10, 5);
    assemble_jacobian<double,double>::print_element_jacobian(iel, Jac, Sol_n_el_dofs, 10, 5);
  
  } //end element loop for each process
  

  
  unsigned int ctrl_index = mlPdeSys->GetSolPdeIndex("control");
  unsigned int mu_index = mlPdeSys->GetSolPdeIndex("mu");

  unsigned int global_ctrl_size = pdeSys->KKoffset[ctrl_index+1][iproc] - pdeSys->KKoffset[ctrl_index][iproc];
  
  std::vector<double>  one_times_mu(global_ctrl_size, 0.);
  std::vector<int>    positions(global_ctrl_size);

  for (unsigned i = 0; i < positions.size(); i++) {
    positions[i] = pdeSys->KKoffset[ctrl_index][iproc] + i;
    one_times_mu[i] = ineq_flag * 1. * (*sol->_Sol[ SolIndex[pos_mu] ])(i/*position_mu_i*/) ;
  }
    RES->add_vector_blocked(one_times_mu, positions);
    
  // ***************** END ASSEMBLY *******************

    if (assembleMatrix) KK->close();
    std::ostringstream mat_out; mat_out << ml_prob.GetFilesHandler()->GetOutputPath() << "/" << "matrix_" << mlPdeSys->GetNonlinearIt()  << ".txt";
    KK->print_matlab(mat_out.str(),"ascii"); //  KK->print();

    RES->close();
    std::ostringstream res_out; res_out << ml_prob.GetFilesHandler()->GetOutputPath() << "/" << "res_" << mlPdeSys->GetNonlinearIt()  << ".txt";
    std::filebuf res_fb;
    res_fb.open (res_out.str().c_str(),std::ios::out);
    std::ostream  res_file_stream(&res_fb);
    RES->print(res_file_stream);


  return;
}


 
  
void ComputeIntegral(const MultiLevelProblem& ml_prob)    {
  
  
  const NonLinearImplicitSystemWithPrimalDualActiveSetMethod* mlPdeSys  = &ml_prob.get_system<NonLinearImplicitSystemWithPrimalDualActiveSetMethod> ("LiftRestr");   // pointer to the linear implicit system named "LiftRestr"
  const unsigned level = mlPdeSys->GetLevelToAssemble();

  Mesh*                    msh = ml_prob._ml_msh->GetLevel(level);    // pointer to the mesh (level) object
  elem*                     el = msh->el;  // pointer to the elem object in msh (level)

  MultiLevelSolution*    ml_sol = ml_prob._ml_sol;  // pointer to the multilevel solution object
  Solution*                sol = ml_prob._ml_sol->GetSolutionLevel(level);    // pointer to the solution (level) object

  const unsigned  dim = msh->GetDimension(); // get the domain dimension of the problem
  unsigned dim2 = (3 * (dim - 1) + !(dim - 1));        // dim2 is the number of second order partial derivatives (1,3,6 depending on the dimension)
  const unsigned max_size = static_cast< unsigned >(ceil(pow(3, dim)));          // conservative: based on line3, quad9, hex27

  unsigned    iproc = msh->processor_id(); // get the process_id (for parallel computation)

 //***************************************************
   const int solType_coords = 0;
  vector < vector < double > > x(dim);
  vector < vector < double> >  coords_at_dofs_bdry(dim);
  for (unsigned i = 0; i < dim; i++) {
         x[i].reserve(max_size);
	 coords_at_dofs_bdry[i].reserve(max_size);
  }
 //*************************************************** 

 //*************************************************** 
  double weight; // gauss point weight
  double weight_bdry = 0.; // gauss point weight on the boundary

 //***************************************************
  double alpha = ALPHA_CTRL_BDRY;
  double beta  = BETA_CTRL_BDRY;
  
 //*************** state ***************************** 
 //*************************************************** 
  vector <double> phi_u;     phi_u.reserve(max_size);             // local test function
  vector <double> phi_u_x;   phi_u_x.reserve(max_size * dim);     // local test function first order partial derivatives
  vector <double> phi_u_xx;  phi_u_xx.reserve(max_size * dim2);   // local test function second order partial derivatives

 
  unsigned solIndex_u;
  solIndex_u = ml_sol->GetIndex("state");    // get the position of "state" in the ml_sol object
  unsigned solType_u = ml_sol->GetSolutionType(solIndex_u);    // get the finite element type for "state"

  vector < double >  sol_u; // local solution
  sol_u.reserve(max_size);
  
  double u_gss = 0.;
 //*************************************************** 
 //***************************************************

  
 //************** desired ****************************
 //***************************************************
  vector <double> phi_udes;  // local test function
  vector <double> phi_udes_x; // local test function first order partial derivatives
  vector <double> phi_udes_xx; // local test function second order partial derivatives

    phi_udes.reserve(max_size);
    phi_udes_x.reserve(max_size * dim);
    phi_udes_xx.reserve(max_size * dim2);
 
  
//  unsigned solIndexTdes;
//   solIndexTdes = ml_sol->GetIndex("Tdes");    // get the position of "state" in the ml_sol object
//   unsigned solTypeTdes = ml_sol->GetSolutionType(solIndexTdes);    // get the finite element type for "state"

  vector < double >  sol_udes; // local solution
  sol_udes.reserve(max_size);

  double udes_gss = 0.;
 //***************************************************
 //***************************************************

 //************** cont *******************************
 //***************************************************
  vector <double> phi_ctrl_bdry;  
  vector <double> phi_ctrl_x_bdry; 

  phi_ctrl_bdry.reserve(max_size);
  phi_ctrl_x_bdry.reserve(max_size * dim);

  unsigned solIndex_ctrl = ml_sol->GetIndex("control");
  unsigned solType_ctrl = ml_sol->GetSolutionType(solIndex_ctrl);

   vector < double >  sol_ctrl;   sol_ctrl.reserve(max_size);
 //***************************************************
 //*************************************************** 
  

 //***************************************************
 //********* WHOLE SET OF VARIABLES ****************** 
  const int solType_max = 2;  //biquadratic
 //***************************************************

  
 //********** DATA *********************************** 
  double u_des = DesiredTarget();
 //*************************************************** 
  
  double integral_target = 0.;
  double integral_alpha  = 0.;
  double integral_beta   = 0.;

    
  // element loop: each process loops only on the elements that owns
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    short unsigned ielGeom = msh->GetElementType(iel);    // element geometry type
    
 //********* GEOMETRY ********************************* 
    unsigned nDofx = msh->GetElementDofNumber(iel, solType_coords);    // number of coordinate element dofs
    for (int i = 0; i < dim; i++)  x[i].resize(nDofx);
    // local storage of coordinates
    for (unsigned i = 0; i < nDofx; i++) {
      unsigned xDof  = msh->GetSolutionDof(i, iel, solType_coords);

      for (unsigned jdim = 0; jdim < dim; jdim++) {
        x[jdim][i] = (*msh->_topology->_Sol[jdim])(xDof);      // global extraction and local storage for the element coordinates
      }
    }

   // elem average point 
    vector < double > elem_center(dim);   
    for (unsigned j = 0; j < dim; j++) {  elem_center[j] = 0.;  }
    for (unsigned j = 0; j < dim; j++) {  
      for (unsigned i = 0; i < nDofx; i++) {
         elem_center[j] += x[j][i];
       }
    }
    
   for (unsigned j = 0; j < dim; j++) { elem_center[j] = elem_center[j]/nDofx; }
 //***************************************************
  
 //****** set target domain flag ********************* 
   int target_flag = 0;
   target_flag = ElementTargetFlag(elem_center);
 //***************************************************

   
 //*********** state ********************************* 
    unsigned nDof_u     = msh->GetElementDofNumber(iel, solType_u);    // number of solution element dofs
    sol_u    .resize(nDof_u);
   // local storage of global mapping and solution
    for (unsigned i = 0; i < sol_u.size(); i++) {
      unsigned solDof_u = msh->GetSolutionDof(i, iel, solType_u);    // global to global mapping between solution node and solution dof
      sol_u[i] = (*sol->_Sol[solIndex_u])(solDof_u);      // global extraction and local storage for the solution
    }
 //*********** state ********************************* 


 //*********** cont ********************************** 
    unsigned nDof_ctrl  = msh->GetElementDofNumber(iel, solType_ctrl);    // number of solution element dofs
    sol_ctrl    .resize(nDof_ctrl);
    for (unsigned i = 0; i < sol_ctrl.size(); i++) {
      unsigned solDof_ctrl = msh->GetSolutionDof(i, iel, solType_ctrl);    // global to global mapping between solution node and solution dof
      sol_ctrl[i] = (*sol->_Sol[solIndex_ctrl])(solDof_ctrl);      // global extraction and local storage for the solution
    } 

 //*********** cont ********************************** 
 
 
 //*********** udes ********************************** 
    unsigned nDof_udes  = msh->GetElementDofNumber(iel, solType_u);    // number of solution element dofs
    sol_udes    .resize(nDof_udes);
    for (unsigned i = 0; i < sol_udes.size(); i++) {
            sol_udes[i] = u_des;  //dof value
    } 
 //*********** udes ********************************** 

 
 //********** ALL VARS ******************************* 
    int nDof_max    =  nDof_u;   //  TODO COMPUTE MAXIMUM maximum number of element dofs for one scalar variable
    
    if(nDof_udes > nDof_max) 
    {
      nDof_max = nDof_udes;
      }
    
    if(nDof_ctrl > nDof_max)
    {
      nDof_max = nDof_ctrl;
    }
    
 //***************************************************

 // ==================================================
 //****** set control flag ***************************
  int control_el_flag = 0;
        control_el_flag = ControlDomainFlag_bdry(elem_center);
  std::vector<int> control_node_flag(nDofx,0);
//   if (control_el_flag == 0) std::fill(control_node_flag.begin(), control_node_flag.end(), 0);
 //***************************************************

  
  	if (control_el_flag == 1) {
	  
	  double tau=0.;
	  vector<double> normal(dim,0);
	       
	  // loop on faces of the current element

	  for(unsigned jface=0; jface < msh->GetElementFaceNumber(iel); jface++) {
          
	    // look for boundary faces
            const int bdry_index = el->GetFaceElementIndex(iel,jface);
            
	    if( bdry_index < 0) {
	      unsigned int face = -( msh->el->GetFaceElementIndex(iel,jface)+1);
	      
		
// 	      if( !ml_sol->_SetBoundaryConditionFunction(xx,"U",tau,face,0.) && tau!=0.){
	      if(  face == FACE_FOR_CONTROL) { //control face

 //========= compute coordinates of boundary nodes on each element ========================================== 
		unsigned nve_bdry = msh->GetElementFaceDofNumber(iel,jface,solType_coords);
	        for (unsigned idim = 0; idim < dim; idim++) {  coords_at_dofs_bdry[idim].resize(nve_bdry); }
		const unsigned felt_bdry = msh->GetElementFaceType(iel, jface);    
		for(unsigned i=0; i < nve_bdry; i++) {
		  unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i);
                  unsigned iDof = msh->GetSolutionDof(i_vol, iel, solType_coords);
		  for(unsigned idim=0; idim<dim; idim++) {
		      coords_at_dofs_bdry[idim][i]=(*msh->_topology->_Sol[idim])(iDof);
		  }
		}
 //========================================================================================================== 
		
		//============ initialize gauss quantities on the boundary ==========================================
                double sol_ctrl_bdry_gss = 0.;
                std::vector<double> sol_ctrl_x_bdry_gss(dim);
		//============ initialize gauss quantities on the boundary ==========================================
		
		for(unsigned ig_bdry=0; ig_bdry < ml_prob.GetQuadratureRule(felt_bdry).GetGaussPointsNumber(); ig_bdry++) {
		  
		  msh->_finiteElement[felt_bdry][solType_ctrl]->JacobianSur(coords_at_dofs_bdry,ig_bdry,weight_bdry,phi_ctrl_bdry,phi_ctrl_x_bdry,normal);

//========== temporary soln for surface gradient on a face parallel to the X axis ===================
          const unsigned int axis_direction_control_side = AXIS_DIRECTION_CONTROL_SIDE;
		  double dx_dcurv_abscissa = 0.;
		 const elem_type_1D * myeltype = static_cast<const elem_type_1D*>(msh->_finiteElement[felt_bdry][solType_ctrl]);
		 double * myptr = myptr = myeltype->GetDPhiDXi(ig_bdry);
		      for (int inode = 0; inode < nve_bdry/*_nc*/; inode++) dx_dcurv_abscissa += myptr[inode] * coords_at_dofs_bdry[axis_direction_control_side][inode];
  
		      for (int inode = 0; inode < nve_bdry/*_nc*/; inode++) {
                            for (int d = 0; d < dim; d++) {
                              if (d == axis_direction_control_side ) phi_ctrl_x_bdry[inode + d*nve_bdry/*_nc*/] = myptr[inode]* (1./ dx_dcurv_abscissa);
                              else  phi_ctrl_x_bdry[inode + d*nve_bdry/*_nc*/] = 0.;
                         }
                     }
//========== temporary soln for surface gradient on a face parallel to the X axis ===================

		  
		 //========== compute gauss quantities on the boundary ===============================================
		  sol_ctrl_bdry_gss = 0.;
                  std::fill(sol_ctrl_x_bdry_gss.begin(), sol_ctrl_x_bdry_gss.end(), 0.);
		      for (int i_bdry = 0; i_bdry < nve_bdry/*_nc*/; i_bdry++)  {
		    unsigned int i_vol = msh->GetLocalFaceVertexIndex(iel, jface, i_bdry);
			
			sol_ctrl_bdry_gss +=  sol_ctrl[i_vol] * phi_ctrl_bdry[i_bdry];
                            for (int d = 0; d < dim; d++) {
			      sol_ctrl_x_bdry_gss[d] += sol_ctrl[i_vol] * phi_ctrl_x_bdry[i_bdry + d*nve_bdry];
			    }
		      }  

                 //========= compute gauss quantities on the boundary ================================================
                  integral_alpha +=  weight * sol_ctrl_bdry_gss * sol_ctrl_bdry_gss; 
                  integral_beta  +=  weight * (sol_ctrl_x_bdry_gss[AXIS_DIRECTION_CONTROL_SIDE] * sol_ctrl_x_bdry_gss[AXIS_DIRECTION_CONTROL_SIDE]);  //in 3D you'll need two directions
                 
		}
	      } //end face == 3
	      
	    } //end if boundary faces
	  }  // loop over element faces   
	  
	} //end if control element flag

//=====================================================================================================================  
//=====================================================================================================================  
//=====================================================================================================================  
  
  
   
      // *** Gauss point loop ***
      for (unsigned ig = 0; ig < ml_prob.GetQuadratureRule(ielGeom).GetGaussPointsNumber(); ig++) {
	
        // *** get gauss point weight, test function and test function partial derivatives ***
	msh->_finiteElement[ielGeom][solType_u]   ->Jacobian(x, ig, weight, phi_u, phi_u_x, phi_u_xx);
    msh->_finiteElement[ielGeom][solType_u/*solTypeTdes*/]->Jacobian(x, ig, weight, phi_udes, phi_udes_x, phi_udes_xx);

	u_gss = 0.;  for (unsigned i = 0; i < nDof_u; i++) u_gss += sol_u[i] * phi_u[i];		
	udes_gss  = 0.; for (unsigned i = 0; i < nDof_udes; i++)  udes_gss  += sol_udes[i]  * phi_udes[i];  

               integral_target += target_flag * weight * (u_gss  - udes_gss) * (u_gss - udes_gss);
	  
      } // end gauss point loop
      
  } //end element loop

  double total_integral = 0.5 * integral_target + 0.5 * alpha * integral_alpha + 0.5 * beta * integral_beta;
  
  std::cout << "The value of the integral_target is " << std::setw(11) << std::setprecision(10) << integral_target << std::endl;
  std::cout << "The value of the integral_alpha  is " << std::setw(11) << std::setprecision(10) << integral_alpha << std::endl;
  std::cout << "The value of the integral_beta   is " << std::setw(11) << std::setprecision(10) << integral_beta << std::endl;
  std::cout << "The value of the total integral  is " << std::setw(11) << std::setprecision(10) << total_integral << std::endl;
 
return;
  
}
  
