#include "mvnfast.h"
#include "internal.h"

/*
 * Fast computation of pdf of a multivariate normal distribution
 *
*/

SEXP rmvnCpp(SEXP n_,  
             SEXP mu_,  
             SEXP sigma_,
             SEXP ncores_,
             SEXP isChol_) 
{ 
    using namespace Rcpp;
    
    try{
      
      RNGScope scope;
      
      int n = as<int>(n_);
      arma::rowvec mu = as<arma::rowvec>(mu_);  
      arma::mat sigma = as<arma::mat>(sigma_); 
      int  ncores = as<int>(ncores_); 
      bool isChol = as<bool>(isChol_); 
      
      int d = mu.n_elem;
    
      // Calculate cholesky dec unless sigma is alread a cholesky dec.
      arma::mat cholDec;
      if( isChol == false ) {
        cholDec = trimatu( arma::chol(sigma) );
      }
      else{
        cholDec = trimatu( sigma );
      }
                  
      // This is the matrix that will be filled with standard normal, we wrap into a arma::mat
      NumericMatrix out(n, d);
      arma::mat tmp( out.begin(), out.nrow(), out.ncol(), false );
      
      // What I to seed the C++11 rng is tricky. I produce "ncores" uniform numbers between 1 and the largest uint64_t,
      // which are the seeds. I put the first one in "coreSeed". If there is no support for OpenMP only this seed
      // will be used, as the computations will be sequential. If there is support for OpenMP "coreSeed" will
      // be over-written, so that each core will get its own seed.
      
      NumericVector seeds = runif(ncores, 1.0, std::numeric_limits<uint64_t>::max());
      
      #ifdef SUPPORT_OPENMP
      #pragma omp parallel num_threads(ncores) if(ncores > 1)
      {
      #endif
      
      double acc;
      int irow, icol, ii;
      arma::rowvec work(d);
      
      uint64_t coreSeed = static_cast<uint64_t>(seeds[0]);
      
      #ifdef SUPPORT_OPENMP
      coreSeed = static_cast<uint64_t>( seeds[omp_get_thread_num()] );
      #endif
      
      std::mt19937_64 engine( coreSeed );
      std::normal_distribution<> normal(0.0, 1.0);
      
      #ifdef SUPPORT_OPENMP
      #pragma omp for schedule(static)
      #endif
      for (irow = 0; irow < n; irow++) 
        for (icol = 0; icol < d; icol++) 
           out(irow, icol) = normal(engine);
      
      #ifdef SUPPORT_OPENMP
      #pragma omp for schedule(static)
      #endif
      for(irow = 0; irow < n; irow++)
      {
       
       for(icol = 0; icol < d; icol++)
       {
        acc = 0.0;
        
        for(ii = 0; ii <= icol; ii++) acc += tmp.at(irow, ii) * cholDec.at(ii, icol);
        
        work.at(icol) = acc;
        
       }
       
       tmp(arma::span(irow), arma::span::all) = work + mu;       
      }
      
      #ifdef SUPPORT_OPENMP
      }
      #endif
      
      return out;
            
    } catch( std::exception& __ex__){
      forward_exception_to_r(__ex__);
    } catch(...){
      ::Rf_error( "c++ exception (unknown reason)" );
    }
}