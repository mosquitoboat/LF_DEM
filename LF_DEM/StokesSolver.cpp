#include "StokesSolver.h"
#ifdef TRILINOS
#include <BelosCGIteration.hpp>
#endif

using namespace std;

/******************************************************
*                                                     *
*                   Public Methods                    *
*                                                     *
******************************************************/

StokesSolver::StokesSolver(int n){
    np=n;
    np3=3*np;
}
StokesSolver::~StokesSolver(){
    #ifdef CHOLMOD
	if (!diag_values)
		delete [] diag_values;
	if (!off_diag_values )
		delete [] off_diag_values;
	if (!ploc)
		delete [] ploc;


	cholmod_free_dense(&chol_solution, &chol_c);
	cholmod_free_dense(&chol_rhs, &chol_c);

	if (brownian){
		cholmod_free_dense(&chol_brownian_rhs, &chol_c);
	}

	cholmod_free_sparse(&chol_rfu_matrix, &chol_c);
	cholmod_finish(&chol_c);
#endif

#ifdef TRILINOS
	for (int i=0; i < linalg_size; i++)
		delete [] columns[i];
	delete [] columns;

	delete [] columns_nb;

	for (int i=0; i < linalg_size; i++)
		delete [] values[i];
	delete [] values;	
#endif

}

void
StokesSolver::initialize(){

#ifdef CHOLMOD
	stype = -1; // 1 is symmetric, stored upper triangular (UT), -1 is LT
	sorted = 0;		/* TRUE if columns sorted, FALSE otherwise*/
	packed = 1;		/* TRUE if matrix packed, FALSE otherwise */
	xtype = CHOLMOD_REAL;
#endif
#ifdef TRILINOS
	RCP<ParameterList> solverParams = parameterList();
	int blocksize = 40;
	int maxiters = 400;
	double tol = 1.e-8;
	solverParams->set( "Block Size", blocksize );              // Blocksize to be used by iterative solver
	solverParams->set( "Maximum Iterations", maxiters );       // Maximum number of iterations allowed
	solverParams->set( "Convergence Tolerance", tol );         // Relative convergence tolerance requested
	solverParams->set( "Verbosity", Belos::Errors + Belos::Warnings );
	tril_solver = tril_factory.create ("CG", solverParams);
#endif

    dof = 3;
    linalg_size = dof*np;

    allocateRessources();
    
}

/************* Matrix filling methods **********************/ 
// Diagonal Terms
void
StokesSolver::addToDiag_RFU(int ii, double alpha){
    
#ifdef CHOLMOD
    int ii6 = 6*ii;
    diag_values[ii6]   += alpha;
    diag_values[ii6+3] += alpha;
    diag_values[ii6+5] += alpha;
#endif
#ifdef TRILINOS
    int iidof = dof*ii;
    for (int j = 0; j < dof; j ++){
	values[iidof+j][j] += alpha; // 00
    }
#endif
}

// Diagonal Blocks Terms
void
StokesSolver::addToDiagBlock_RFU(const vec3d &nvec, int ii, double alpha){
    double alpha_n0 = alpha*nvec.x;
    double alpha_n1 = alpha*nvec.y;
    double alpha_n2 = alpha*nvec.z;
    double alpha_n1n0 = alpha_n0*nvec.y;
    double alpha_n2n1 = alpha_n1*nvec.z;
    double alpha_n0n2 = alpha_n2*nvec.x;
    
#ifdef CHOLMOD
    int ii6 = 6*ii;
    diag_values[ii6]   += alpha_n0*nvec.x; // 00
    diag_values[ii6+1] += alpha_n1n0; // 10
    diag_values[ii6+2] += alpha_n0n2; // 20
    
    diag_values[ii6+3] += alpha_n1*nvec.y; //11
    diag_values[ii6+4] += alpha_n2n1; // 21
    
    diag_values[ii6+5] += alpha_n2*nvec.z; //22
#endif
#ifdef TRILINOS
    int iidof = dof*ii;
    values[iidof  ][0] += alpha_n0*nvec.x; // 00
    values[iidof  ][1] += alpha_n1n0; // 01
    values[iidof  ][2] += alpha_n0n2; // 02
    
    values[iidof+1][0] += alpha_n1n0; // 10
    values[iidof+1][1] += alpha_n1*nvec.y; // 11
    values[iidof+1][2] += alpha_n2n1; // 12
    
    values[iidof+2][0] += alpha_n0n2; // 20
    values[iidof+2][1] += alpha_n2n1; // 21
    values[iidof+2][2] += alpha_n2*nvec.z; // 20
#endif
}

// Off-Diagonal Blocks Terms
void
StokesSolver::appendToOffDiagBlock_RFU(const vec3d &nvec, int ii, int jj, double alpha){

#ifdef CHOLMOD
    appendToColumn_RFU(nvec, ii, jj, alpha);
#endif    
#ifdef TRILINOS
    appendToRow_RFU(nvec, ii, jj, alpha);
#endif
}


#ifdef CHOLMOD
/*************** Cholmod Matrix Filling *************
Cholmod matrices we are using are defined in column major order (index j is column index)

Cholmod matrices are defined as follows:
- all values are stored in array x ( size nzmax )
- locations of values are encoded in array p ( size np ):
  values corresponding to column j are x[ p[j] ]  to x[ p[j+1] - 1 ]
- corresponding rows are stored in array i ( size nzmax ):
  rows corresponding to column j are i[ p[j] ]  to i[ p[j+1] - 1 ]

Hence: 
with p[j] < a < p[j+1]-1  
           . . . . j . . . . . .
        .|         .            |
        .|         .            |
        .|         .            |  
     i[a]| . . . .x[a]          |
        .|                      |
        .|                      |


*****************************************************/
void
StokesSolver::complete_RFU(){

    // declare the last 2 values of ploc
    ploc[np-1] = (unsigned int)rows.size();
    ploc[np] = (unsigned int)rows.size();
    
    allocate_RFU();
    
    // fill
    for(int j = 0; j < np; j++){
	int j3 = 3*j;
	int j6 = 6*j;
	
	((int*)chol_rfu_matrix->p)[j3  ] = j6   + 3*ploc[j];
	((int*)chol_rfu_matrix->p)[j3+1] = j6+3 + 2*ploc[j] +   ploc[j+1];
	((int*)chol_rfu_matrix->p)[j3+2] = j6+5 +   ploc[j] + 2*ploc[j+1];
	
	int pj3   = ((int*)chol_rfu_matrix->p)[j3];
	int pj3_1 = ((int*)chol_rfu_matrix->p)[j3+1];
	int pj3_2 = ((int*)chol_rfu_matrix->p)[j3+2];
	
	// diagonal blocks row indices
	((int*)chol_rfu_matrix->i)[ pj3 ]       = j3;
	((int*)chol_rfu_matrix->i)[ pj3 + 1 ]   = j3+1;
	((int*)chol_rfu_matrix->i)[ pj3 + 2 ]   = j3+2;
	
	((int*)chol_rfu_matrix->i)[ pj3_1 ]     = j3+1;
	((int*)chol_rfu_matrix->i)[ pj3_1 + 1 ] = j3+2;
	
	((int*)chol_rfu_matrix->i)[ pj3_2 ]     = j3+2;
	
	// diagonal blocks row values
	((double*)chol_rfu_matrix->x)[ pj3 ]       = diag_values[j6];
	((double*)chol_rfu_matrix->x)[ pj3 + 1 ]   = diag_values[j6+1];
	((double*)chol_rfu_matrix->x)[ pj3 + 2 ]   = diag_values[j6+2];
	
	((double*)chol_rfu_matrix->x)[ pj3_1 ]     = diag_values[j6+3];
	((double*)chol_rfu_matrix->x)[ pj3_1 + 1 ] = diag_values[j6+4];
	
	((double*)chol_rfu_matrix->x)[ pj3_2 ]     = diag_values[j6+5];
	
	// off-diagonal blocks row indices and values
	for(int k = ploc[j]; k < ploc[j+1]; k++){
	    int u = k - ploc[j];
	    ((int*)chol_rfu_matrix->i)[ pj3   + u + 3 ] = rows[k];
	    ((int*)chol_rfu_matrix->i)[ pj3_1 + u + 2 ] = rows[k];
	    ((int*)chol_rfu_matrix->i)[ pj3_2 + u + 1 ] = rows[k];
	    
	    ((double*)chol_rfu_matrix->x)[ pj3   + u + 3 ] = off_diag_values[0][k];
	    ((double*)chol_rfu_matrix->x)[ pj3_1 + u + 2 ] = off_diag_values[1][k];
	    ((double*)chol_rfu_matrix->x)[ pj3_2 + u + 1 ] = off_diag_values[2][k];
	}
    }
    ((int*)chol_rfu_matrix->p)[np3] = ((int*)chol_rfu_matrix->p)[np3-1] + 1;

    // for(int i = 0; i < linalg_size; i++){
    //   for(int k =((int*)chol_rfu_matrix->p)[i] ; k < ((int*)chol_rfu_matrix->p)[i+1]; k++){
    //     cout << i << " " << ((int*)chol_rfu_matrix->i)[k] << " " << ((double*)chol_rfu_matrix->x)[k] << endl;
    //   }
    // }
    
}
#endif

#ifdef TRILINOS
/*************** Epetra_CrsMatrix Filling *************
Epetra_CrsMatrix we are using are defined in row major order.

Epetra_CrsMatrix must be stored completely.

Epetra_CrsMatrix elements are not accessed directly for filling.
Instead we use user friendly methods, that take one row at a time.

*****************************************************/

void
StokesSolver::complete_RFU(){

    for(int i = 0; i < linalg_size; i++){
	tril_rfu_matrix->InsertGlobalValues(i, columns_nb[i] , values[i], columns[i]);
    }
    // FillComplete matrix before building the preconditioner
    tril_rfu_matrix->FillComplete();
    
    buildDiagBlockPreconditioner();

    tril_l_precond->FillComplete();
}
#endif


void
StokesSolver::prepareNewBuild_RFU(){

#ifdef TRILINOS
    tril_rfu_matrix = new Epetra_CrsMatrix(Copy, *Map, 12*dof + dof );
    tril_l_precond = new Epetra_CrsMatrix(Copy, *Map, 3);
    tril_rfu_matrix->PutScalar(0.);

    for (int i = 0; i < linalg_size; i++){
	for (int j = 0; j < columns_max_nb; j++){
	    columns[i][j] = -1;
	    values[i][j] = 0.;
	}
    }
    
    // declare the diagonal blocks
    for (int i = 0; i < np; i++){
	int idof = dof*i;
	for (int j = 0; j < dof; j++){
	    columns[idof  ][j] = idof+j;
	    columns[idof+1][j] = idof+j;
	    columns[idof+2][j] = idof+j;
	}
	columns_nb[idof  ] = dof;
	columns_nb[idof+1] = dof;
	columns_nb[idof+2] = dof;
    }
 
#endif

#ifdef CHOLMOD
    last_updated_colblock = -1;
    for (int k = 0; k < 6*np; k++){
	diag_values[k] = 0.;
    }
    
    rows.clear();
    off_diag_values[0].clear();
    off_diag_values[1].clear();
    off_diag_values[2].clear();
#endif

}


void
StokesSolver::resetRHS(){
#ifdef CHOLMOD
    for (int i = 0; i < linalg_size; i++){
	((double*)chol_rhs->x)[ i ] = 0.;
    }
#endif
#ifdef TRILINOS
    tril_rhs->PutScalar(0.);
#endif

}

void
StokesSolver::addToRHS(int i, double val){

#ifdef TRILINOS
    tril_rhs->SumIntoMyValue( i, 0, val);
#endif

#ifdef CHOLMOD
    ((double*)chol_rhs->x)[ i ] += val;
#endif

}


void
StokesSolver::setRHS(double* rhs){
#ifdef CHOLMOD
    for (int i = 0; i < linalg_size; i++){
	((double*)chol_rhs->x)[i] = rhs[i];
    }
#endif
#ifdef TRILINOS
    cerr << " Error : StokesSolver::setRHS(double* rhs) not implemented for TRILINOS yet ! " << endl;
    exit(1);
#endif
}


void
StokesSolver::solve(double* velocity){
#ifdef CHOLMOD
    factorizeRFU();
    chol_solution = cholmod_solve (CHOLMOD_A, chol_L, chol_rhs, &chol_c) ;
    for (int i = 0; i < linalg_size; i++){
	velocity[i] = ((double*)chol_solution->x)[i];
    }
#endif

#ifdef TRILINOS
    RCP < Belos::LinearProblem < SCAL, VEC, MAT > > tril_stokes_equation = rcp( new Belos::LinearProblem < SCAL, VEC, MAT > ( rcp ( tril_rfu_matrix, false), tril_solution, tril_rhs ) );
    
    tril_stokes_equation->setLeftPrec( rcp ( tril_l_precond, false) );

    bool set_success = tril_stokes_equation->setProblem();
    if(!set_success){
	cerr << "ERROR:  Belos::LinearProblem failed to set up correctly" << endl;
	exit(1);
    }
    tril_solver->setProblem (tril_stokes_equation);
    Belos::ReturnType ret = tril_solver->solve();
    if( ret != Belos::Converged )
	cerr << " Warning: Belos::Solver did not converge" << endl;
    int iter_steps = tril_solver->getNumIters();
    cout << " iterations " << iter_steps << endl;

    tril_solution->ExtractCopy(&velocity);

#endif

}

void
StokesSolver::solvingIsDone(){
    #ifdef CHOLMOD
	cholmod_free_factor(&chol_L, &chol_c);
	cholmod_free_sparse(&chol_rfu_matrix, &chol_c);
	//	cholmod_free_dense(&chol_rhs, &chol_c);
	cholmod_free_dense(&chol_solution, &chol_c);
#endif
#ifdef TRILINOS
	delete tril_rfu_matrix;
	delete tril_l_precond;
#endif

}



/******************************************************
*                                                     *
*                  Private Methods                    *
*                                                     *
******************************************************/

void
StokesSolver::allocateRessources(){
	
#ifdef TRILINOS
    int maxnum_interactionpair_per_particle = 15;
    columns_max_nb = dof*maxnum_interactionpair_per_particle;
    int numlhs = 1;
    int numrhs = 1;
    Map = rcp(new Epetra_Map(linalg_size, 0, Comm));
    tril_solution = rcp( new VEC(*Map, numlhs) );
    tril_rhs = rcp( new VEC(*Map, numrhs) );
    //	tril_rfu_matrix = rcp( new MAT(linalg_size) );
    
    columns = new int* [linalg_size];
    for (int i=0; i < linalg_size; i++){
	columns[i] = new int [columns_max_nb];
	for(int j=0; j < columns_max_nb; j++){
	    columns[i][j] = -1;
	}
    }
    values = new double* [linalg_size];
    for (int i=0; i < linalg_size; i++){
	values[i] = new double [columns_max_nb];
	for(int j=0; j < columns_max_nb; j++){
	    values[i][j] = 0.;
	}
    }
    columns_nb = new int [linalg_size];
    for (int i=0; i < linalg_size; i++)
	columns_nb[i] = 0;
    
    
#endif
#ifdef CHOLMOD
    cholmod_start (&chol_c) ;
    diag_values = new double [6*np];
    off_diag_values = new vector <double> [3];
    ploc = new int [np+1];
    chol_rhs = cholmod_allocate_dense(np3, xtype, &chol_c);
    chol_L=NULL;
#endif

}

#ifdef CHOLMOD
void
StokesSolver::allocate_RFU(){
	// allocate
	int nzmax;  // non-zero values
	nzmax = 6*np; // diagonal blocks
	for(int s=0; s<3; s++){
		nzmax += off_diag_values[s].size();  // off-diagonal
	}
	chol_rfu_matrix = cholmod_allocate_sparse(np3, np3, nzmax, sorted, packed, stype,xtype, &chol_c);
}
#endif


#ifdef CHOLMOD
void
StokesSolver::appendToColumn_RFU(const vec3d &nvec, int ii, int jj, double alpha){

    if(ii>last_updated_colblock){
	last_updated_colblock = ii;
	ploc[i] = (unsigned int)rows.size();
    }

    int jj3   = 3*jj;
    int jj3_1 = jj3+1;
    int jj3_2 = jj3+2;
    double alpha_n0 = alpha*nvec.x;
    double alpha_n1 = alpha*nvec.y;
    double alpha_n2 = alpha*nvec.z;
    double alpha_n1n0 = alpha_n0*nvec.y;
    double alpha_n2n1 = alpha_n1*nvec.z;
    double alpha_n0n2 = alpha_n2*nvec.x;
    
    rows.push_back(jj3);
    rows.push_back(jj3_1);
    rows.push_back(jj3_2);
    
    off_diag_values[0].push_back(alpha_n0*nvec.x); // 00
    off_diag_values[0].push_back(alpha_n1n0); // 10
    off_diag_values[0].push_back(alpha_n0n2); // 20
    off_diag_values[1].push_back(alpha_n1n0); // 01
    off_diag_values[1].push_back(alpha_n1*nvec.y); //11
    off_diag_values[1].push_back(alpha_n2n1); // 21
    off_diag_values[2].push_back(alpha_n0n2); // 02
    off_diag_values[2].push_back(alpha_n2n1); // 12
    off_diag_values[2].push_back(alpha_n2*nvec.z); //22
}
#endif

#ifdef TRILINOS
void
StokesSolver::appendToRow_RFU(const vec3d &nvec, int ii, int jj, double alpha){
    int ii3   = 3*ii;
    int ii3_1 = ii3+1;
    int ii3_2 = ii3+2;
    int jj3   = 3*jj;
    int jj3_1 = jj3+1;
    int jj3_2 = jj3+2;
    
    double alpha_n0 = alpha*nvec.x;
    double alpha_n1 = alpha*nvec.y;
    double alpha_n2 = alpha*nvec.z;
    double alpha_n1n0 = alpha_n0*nvec.y;
    double alpha_n2n1 = alpha_n1*nvec.z;
    double alpha_n0n2 = alpha_n2*nvec.x;
    
    
    // declare ii and jj new columns, and update column nb
    int last_col_nb_ii = columns_nb[ii3];
    int last_col_nb_jj = columns_nb[jj3];
    // cout << " last_col_nb_ii " << last_col_nb_ii << endl;
    // cout << " last_col_nb_jj " << last_col_nb_jj << endl;
    columns[ii3  ][last_col_nb_ii  ] = jj3  ;
    columns[ii3  ][last_col_nb_ii+1] = jj3_1;
    columns[ii3  ][last_col_nb_ii+2] = jj3_2;
    columns[ii3_1][last_col_nb_ii  ] = jj3  ;
    columns[ii3_1][last_col_nb_ii+1] = jj3_1;
    columns[ii3_1][last_col_nb_ii+2] = jj3_2;
    columns[ii3_2][last_col_nb_ii  ] = jj3  ;
    columns[ii3_2][last_col_nb_ii+1] = jj3_1;
    columns[ii3_2][last_col_nb_ii+2] = jj3_2;
    
    columns[jj3  ][last_col_nb_jj  ] = ii3  ;
    columns[jj3  ][last_col_nb_jj+1] = ii3_1;
    columns[jj3  ][last_col_nb_jj+2] = ii3_2;
    columns[jj3_1][last_col_nb_jj  ] = ii3  ;
    columns[jj3_1][last_col_nb_jj+1] = ii3_1;
    columns[jj3_1][last_col_nb_jj+2] = ii3_2;
    columns[jj3_2][last_col_nb_jj  ] = ii3  ;
    columns[jj3_2][last_col_nb_jj+1] = ii3_1;
    columns[jj3_2][last_col_nb_jj+2] = ii3_2;
    
    columns_nb[ii3] += 3;
    columns_nb[ii3_1] += 3;
    columns_nb[ii3_2] += 3;
    columns_nb[jj3] += 3;
    columns_nb[jj3_1] += 3;
    columns_nb[jj3_2] += 3;
    
    // set values	
    values[ii3  ][last_col_nb_ii  ] = alpha_n0*nvec.x; // 00
    values[ii3  ][last_col_nb_ii+1] = alpha_n1n0;      // 01
    values[ii3  ][last_col_nb_ii+2] = alpha_n0n2;      // 02
    values[ii3_1][last_col_nb_ii  ] = alpha_n1n0;      // 10
    values[ii3_1][last_col_nb_ii+1] = alpha_n1*nvec.y; // 11
    values[ii3_1][last_col_nb_ii+2] = alpha_n2n1;      // 12
    values[ii3_2][last_col_nb_ii  ] = alpha_n0n2;      // 20
    values[ii3_2][last_col_nb_ii+1] = alpha_n2n1;      // 21
    values[ii3_2][last_col_nb_ii+2] = alpha_n2*nvec.z;      // 22
    
    values[jj3  ][last_col_nb_jj  ] = alpha_n0*nvec.x; // 00
    values[jj3  ][last_col_nb_jj+1] = alpha_n1n0;      // 01
    values[jj3  ][last_col_nb_jj+2] = alpha_n0n2;      // 02
    values[jj3_1][last_col_nb_jj  ] = alpha_n1n0;      // 10
    values[jj3_1][last_col_nb_jj+1] = alpha_n1*nvec.y; // 11
    values[jj3_1][last_col_nb_jj+2] = alpha_n2n1;      // 12
    values[jj3_2][last_col_nb_jj  ] = alpha_n0n2;      // 20
    values[jj3_2][last_col_nb_jj+1] = alpha_n2n1;      // 21
    values[jj3_2][last_col_nb_jj+2] = alpha_n2*nvec.z;      // 22
    
}
#endif



#ifdef CHOLMOD
void
System::factorizeRFU(){
    chol_L = cholmod_analyze (chol_rfu_matrix, &chol_c);
    cholmod_factorize (chol_rfu_matrix, chol_L, &chol_c);
    if(chol_c.status){
	// Cholesky decomposition has failed: usually because matrix is incorrectly found to be positive-definite
	// It is very often enough to force another preconditioner to solve the problem.
	cerr << " factorization failed. forcing simplicial algorithm... " << endl;
	chol_c.supernodal = CHOLMOD_SIMPLICIAL;
	chol_L = cholmod_analyze (chol_rfu_matrix, &chol_c);
	cholmod_factorize (chol_rfu_matrix, chol_L, &chol_c) ;
	cerr << " factorization status " << chol_c.status << " final_ll ( 0 is LDL, 1 is LL ) " <<  chol_c.final_ll <<endl;
	chol_c.supernodal = CHOLMOD_SUPERNODAL;
    }
    
}
#endif


/*************  Preconditioners *********************/

/* 
  buildDiagBlockPreconditioner() :
  A block diagonal (left-)preconditioner, almost similar to the 
  one described in Amit Kumar's PhD Thesis: it is zero everywhere
  except along the diagonal where diagonal 3x3 block are the 
  ones of R_FU:

          ..........   
        |          .                             |
        | R_FU(i,j).     0                       |
        |          .                             |  
        | .....................                  |
        |          .          .                  |
        |     0    . R_FU(i,j).                  |
        |          .          .                  |  
        |          ............                  |
  P =   |                       .                |
        |                         .              |
        |                           .            |
        |                             ...........|           
        |                             .          |
        |                             . R_FU(i,j)|                   |
        |                             .          |
                                      ...........

   This method stores P^{-1} in tril_l_precond.
*/
void
StokesSolver::buildDiagBlockPreconditioner(){
    
    double a00, a01, a02, a11, a12, a22;
    double det, idet;
    double *precond_row = new double [3];
    int *indices = new int [3];
    for(int i = 0; i < np; i++){
	int i3 = 3*i;
	
	indices[0] = i3;
	indices[1] = i3 + 1;
	indices[2] = i3 + 2;
	
	    // +2.5*r in the diagonal ? --> Amit Kumar, PhD Thesis
	a00 = values[i3][0];
	a01 = values[i3][1];
	a02 = values[i3][2];
	a11 = values[i3+1][1];
	a12 = values[i3+1][2];
	a22 = values[i3+2][2];
	
	det = a00*(a22*a11-a12*a12) + a01*(-a01*a22+2*a12*a02 )-a02*a02*a11;
	idet = 1./det;
	
	// row i3
	precond_row[0] = idet*(a11*a22-a12*a12);
	precond_row[1] = idet*(a02*a12-a01*a22);
	precond_row[2] = idet*(a01*a12-a02*a11);
	
	tril_l_precond->InsertGlobalValues(i3, 3 , precond_row, indices);
	
	// row i3+1
	precond_row[0] = precond_row[1]; // symmetric matrix!
	precond_row[1] = idet*(a00*a22-a02*a02);
	precond_row[2] = idet*(a02*a01-a00*a12);
	
	tril_l_precond->InsertGlobalValues(i3+1, 3 , precond_row, indices);
	
	// row i3+2
	precond_row[1] = precond_row[2]; // symmetric matrix!
	precond_row[0] = idet*(a01*a12-a02*a11);
	precond_row[2] = idet*(a00*a11-a01*a01);

	tril_l_precond->InsertGlobalValues(i3+2, 3 , precond_row, indices);
	
    }
    
    delete [] precond_row;
    delete [] indices;
    
}

/* 
  buildIncCholPreconditioner() :
  A incomplete Cholesky factorization (left-)preconditioner.
*/
void
StokesSolver::buildIncCholPreconditioner(){
//  parameters to be tuned
    int fill_level = 1;
    double drop_tolerance = 0.0;

    // Ifpack_CrsIct * ICT = NULL;
    // ICT = new Ifpack_CrsIct(tril_rfu_matrix, drop_tolerance, fill_level);
    
}