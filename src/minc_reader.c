#include <minc2.h>
#include <stdio.h>
#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>
#include <R_ext/Utils.h>

/* compiling:
gcc -shared -fPIC -I/usr/lib/R/include/ -I/projects/mice/share/arch/linux64/include -L/projects/mice/share/arch/linux64/lib -o minc_reader.so minc_reader.c -lminc2 -lhdf5 -lnetcdf -lz -lm
*/

void get_volume_sizes(char **filename, unsigned int *sizes) {
  int result;
  mihandle_t  hvol;
  unsigned long tmp_sizes[3];
  midimhandle_t dimensions[3];
   /* open the existing volume */
  result = miopen_volume(filename[0],
			 MI2_OPEN_READ, &hvol);
  if (result != MI_NOERROR) {
    error("Error opening input file: %s.\n", filename[0]);
  }

    /* get the file dimensions and their sizes */
  miget_volume_dimensions( hvol, MI_DIMCLASS_SPATIAL,
			   MI_DIMATTR_ALL, MI_DIMORDER_FILE,
			   3, dimensions);
  result = miget_dimension_sizes( dimensions, 3, tmp_sizes ); 
  Rprintf("Sizes: %i %i %i\n", tmp_sizes[0], tmp_sizes[1], tmp_sizes[2]);
  sizes[0] = (unsigned int) tmp_sizes[0];
  sizes[1] = (unsigned int) tmp_sizes[1];
  sizes[2] = (unsigned int) tmp_sizes[2];
  return;
}

/* get a voxel from all files */
void get_voxel_from_files(char **filenames, int *num_files,
			  int *v1, int *v2, int *v3, double *voxel) {
  unsigned long location[3];
  mihandle_t hvol;
  int result;
  int i;

  location[0] = *v1;
  location[1] = *v2;
  location[2] = *v3;

  for(i=0; i < *num_files; i++) {
    /* open the volume */
    result = miopen_volume(filenames[i],
			   MI2_OPEN_READ, &hvol);
    if (result != MI_NOERROR) {
      error("Error opening input file: %s.\n", filenames[i]);
    }

    result = miget_real_value(hvol, location, 3, &voxel[i]);

    if (result != MI_NOERROR) {
      error("Error getting voxel from: %s.\n", filenames[i]);
    }
    miclose_volume(hvol);
  }
}
  

/* get a real value hyperslab from file */
void get_hyperslab(char **filename, int *start, int *count, double *slab) {
  int                result;
  mihandle_t         hvol;
  int                i;
  unsigned long      tmp_start[3];
  unsigned long      tmp_count[3];

  /* open the volume */
  result = miopen_volume(filename[0],
			 MI2_OPEN_READ, &hvol);
  if (result != MI_NOERROR) {
    error("Error opening input file: %s.\n", filename[0]);
  }

  for (i=0; i < 3; i++) {
    tmp_start[i] = (unsigned long) start[i];
    tmp_count[i] = (unsigned long) count[i];
  }

  /* get the hyperslab */
  Rprintf("Start: %i %i %i\n", start[0], start[1], start[2]);
  Rprintf("Count: %i %i %i\n", count[0], count[1], count[2]);
  if (miget_real_value_hyperslab(hvol, 
				 MI_TYPE_DOUBLE, 
				 (unsigned long *) tmp_start, 
				 (unsigned long *) tmp_count, 
				 slab)
      < 0) {
    error("Could not get hyperslab.\n");
  }
  return;
}

/* get a real value hyperslab from file */
SEXP get_hyperslab2( SEXP filename,  SEXP start,  SEXP count, SEXP slab) {
  int                result;
  mihandle_t         hvol;
  int                i;
  unsigned long      tmp_start[3];
  unsigned long      tmp_count[3];

  /*
  char **c_filename;
  int *c_start;
  int *c_count;
  double *c_slab;
  */
  /* open the volume */
  
  Rprintf("Crap %s\n", CHAR(STRING_ELT(filename, 0)));
  result = miopen_volume(CHAR(STRING_ELT(filename,0)),
			 MI2_OPEN_READ, &hvol);
  if (result != MI_NOERROR) {
    error("Error opening input file: %s.\n", CHAR(STRING_ELT(filename,0)));
  }

  for (i=0; i < 3; i++) {
    tmp_start[i] = (unsigned long) INTEGER(start)[i];
    tmp_count[i] = (unsigned long) INTEGER(count)[i];
  }

  /* get the hyperslab */
  Rprintf("Start: %i %i %i\n", INTEGER(start)[0], INTEGER(start)[1], INTEGER(start)[2]);
  Rprintf("Count: %i %i %i\n", INTEGER(count)[0], INTEGER(count)[1], INTEGER(count)[2]);
  if (miget_real_value_hyperslab(hvol, 
				 MI_TYPE_DOUBLE, 
				 (unsigned long *) tmp_start, 
				 (unsigned long *) tmp_count, 
				 REAL(slab))
      < 0) {
    error("Could not get hyperslab.\n");
  }
  return(slab);
}

SEXP t_test(SEXP voxel, SEXP grouping, SEXP n) {
  double *xvoxel, *xgrouping, *xn, *t;
  double x_mean, x_var, y_mean, y_var, x_sum, y_sum, x_n, y_n, s_p;
  int i;
  SEXP output;

  xgrouping = REAL(grouping);
  xvoxel = REAL(voxel);
  xn = REAL(n);

  PROTECT(output=allocVector(REALSXP, 1));
  t = REAL(output);

  /* compute sums and Ns */
  x_sum = 0;
  y_sum = 0;
  x_n = 0;
  y_n = 0;
  for (i=0; i < xn[0]; i++) {
    if (xgrouping[i] == 0) {
      x_n++;
      x_sum += xvoxel[i];
    }
    else if (xgrouping[i] == 1) {
      y_n++;
      y_sum += xvoxel[i];
    }
    else {
      error("Grouping value not 0 or 1\n");
    }
  }

  if (x_n == 0 || y_n == 0) {
    error("Each group must contain at least one subject\n");
  }

  x_mean = x_sum / x_n;
  y_mean = y_sum / y_n;

  x_var = 0;
  y_var = 0;

  /* compute the variances */
  for (i=0; i < xn[0]; i++) {
    if (xgrouping[i] == 0) {
      x_var += pow(xvoxel[i] - x_mean, 2);
    }
    else if (xgrouping[i] == 1) {
      y_var += pow(xvoxel[i] - y_mean, 2);
    }
  }
  x_var = x_var / x_n;
  y_var = y_var / y_n;

  /*
  Rprintf("Var (x,y) = %f %f\nMean (x,y) = %f %f\nN (x,y) %f %f\n", 
	  x_var, y_var, x_mean, y_mean, x_n, y_n);
  */

  //s_p = ( ((x_n - 1) * x_var) + ((y_n - 1) * y_var) ) / x_n + y_n - 2;
  t[0] = ( x_mean - y_mean ) / sqrt( (x_var / (x_n-1) ) + (y_var / (y_n-1)));

  UNPROTECT(1);
  return(output);
}

/* wilcoxon_rank_test: wilcoxon rank test (Mann-Whitney U test)
 * voxel: 1D array of doubles
 * grouping: 1D array of doubles
 * the test is against grouping == 0
 * NOTES:
 * does not handle ties in rankings correctly.
 */

SEXP wilcoxon_rank_test(SEXP voxel, SEXP grouping, SEXP na, SEXP nb) {
  double *xvoxel, *voxel_copy, *xgrouping, rank_sum, expected_rank_sum, *xW;
  double *xna, *xnb;
  int    *xindices;
  int *index;
  unsigned long    n, i;
  SEXP   indices, output;

  xgrouping = REAL(grouping);

  PROTECT(output=allocVector(REALSXP, 1));
  xW = REAL(output);
  xna = REAL(na);
  xnb = REAL(nb);

  //Rprintf("NA: %f NB: %f\n", *xna, *xnb);
  expected_rank_sum = *xna * *xnb + ((*xna * (*xna + 1)) / 2);

  n = LENGTH(voxel);
  xvoxel = REAL(voxel);
  voxel_copy = malloc(n * sizeof(double));
  index = malloc(n * sizeof(int));
  for (i=0; i < n; i++) {
    voxel_copy[i] = xvoxel[i];
    index[i] = i;
  }

  PROTECT(indices=allocVector(INTSXP, n));
  xindices = INTEGER(indices);

  rsort_with_index(voxel_copy, (int*) index, n);
  rank_sum = 0;
  
  for (i=0; i < n; i++) {
    //Rprintf("Index %d: %d %f\n", i, index[i]+1, xgrouping[i]);
    xindices[index[i]] = i+1;
    //if (xgrouping[i] == 0) 
    //expected_rank_sum += i+1;
  }
  for (i=0;  i< n; i++) {
    if (xgrouping[i] == 0)
      rank_sum += xindices[i];
  }
  //Rprintf("RANK SUM: %f\nEXPECTED SUM: %f\nW: %f\n", 
  //  rank_sum, expected_rank_sum, expected_rank_sum - rank_sum);
  xW[0] = expected_rank_sum - rank_sum;
  free(voxel_copy);
  free(index);
  UNPROTECT(2);
  return(output);
}
  

/* minc2_apply: evaluate a function at every voxel of a series of files
 * filenames: character array of filenames. Have to have identical sampling.
 * fn: string representing a function call to be evaluated. The variable "x"
 *     will be a vector of length number_volumes in the same order as the 
 *     filenames array.
 * rho: the R environment.
 */
     
SEXP minc2_apply(SEXP filenames, SEXP fn, SEXP have_mask, 
		 SEXP mask, SEXP rho) {
  int                result;
  mihandle_t         *hvol, hmask;
  int                i, v0, v1, v2, output_index, buffer_index;
  unsigned long      start[3], count[3];
  unsigned long      location[3];
  int                num_files;
  double             *xbuffer, *xoutput, **full_buffer, *xhave_mask;
  double             *mask_buffer;
  midimhandle_t      dimensions[3];
  unsigned long      sizes[3];
  SEXP               output, buffer, R_fcall;
  

  /* allocate memory for volume handles */
  num_files = LENGTH(filenames);
  hvol = malloc(num_files * sizeof(mihandle_t));

  Rprintf("Number of volumes: %i\n", num_files);

  /* open the mask - if so desired */
  xhave_mask = REAL(have_mask);
  if (xhave_mask[0] == 1) {
    result = miopen_volume(CHAR(STRING_ELT(mask, 0)),
			   MI2_OPEN_READ, &hmask);
    if (result != MI_NOERROR) {
      error("Error opening mask: %s.\n", CHAR(STRING_ELT(mask, 0)));
    }
  }
  

  /* open each volume */
  for(i=0; i < num_files; i++) {
    result = miopen_volume(CHAR(STRING_ELT(filenames, i)),
      MI2_OPEN_READ, &hvol[i]);
    if (result != MI_NOERROR) {
      error("Error opening input file: %s.\n", CHAR(STRING_ELT(filenames,i)));
    }
  }

  /* get the file dimensions and their sizes - assume they are the same*/
  miget_volume_dimensions( hvol[0], MI_DIMCLASS_SPATIAL,
			   MI_DIMATTR_ALL, MI_DIMORDER_FILE,
			   3, dimensions);
  result = miget_dimension_sizes( dimensions, 3, sizes );
  Rprintf("Volume sizes: %i %i %i\n", sizes[0], sizes[1], sizes[2]);

  /* allocate the output buffer */
  PROTECT(output=allocVector(REALSXP, (sizes[0] * sizes[1] * sizes[2])));
  xoutput = REAL(output);

  /* allocate the local buffer that will be passed to the function */
  PROTECT(buffer=allocVector(REALSXP, num_files));
  xbuffer = REAL(buffer); 

  //PROTECT(R_fcall = lang2(fn, R_NilValue));


  /* allocate first dimension of the buffer */
  full_buffer = malloc(num_files * sizeof(double));

  /* allocate second dimension of the buffer 
     - big enough to hold one slice per subject at a time */
  for (i=0; i < num_files; i++) {
    full_buffer[i] = malloc(sizes[1] * sizes[2] * sizeof(double));
  }
  
  /* allocate buffer for mask - if necessary */
  if (xhave_mask[0] == 1) {
    mask_buffer = malloc(sizes[1] * sizes[2] * sizeof(double));
  }
	
  /* set start and count. start[0] will change during the loop */
  start[0] = 0; start[1] = 0; start[2] = 0;
  count[0] = 1; count[1] = sizes[1]; count[2] = sizes[2];

  /* loop across all files and voxels */
  Rprintf("In slice \n");
  for (v0=0; v0 < sizes[0]; v0++) {
    start[0] = v0;
    for (i=0; i < num_files; i++) {
      if (miget_real_value_hyperslab(hvol[i], 
				     MI_TYPE_DOUBLE, 
				     (unsigned long *) start, 
				     (unsigned long *) count, 
				     full_buffer[i]) )
	error("Error opening buffer.\n");
    }
    /* get mask - if desired */
    if (xhave_mask[0] == 1) {
      if (miget_real_value_hyperslab(hmask, 
				     MI_TYPE_DOUBLE, 
				     (unsigned long *) start, 
				     (unsigned long *) count, 
				     mask_buffer) )
	error("Error opening mask buffer.\n");
    }

    Rprintf(" %d ", v0);
    for (v1=0; v1 < sizes[1]; v1++) {
      for (v2=0; v2 < sizes[2]; v2++) {
	output_index = v0*sizes[1]*sizes[2]+v1*sizes[2]+v2;
	buffer_index = sizes[2] * v1 + v2;

	/* only perform operation if not masked */
	if(xhave_mask[0] == 0 
	   || (xhave_mask[0] == 1 && mask_buffer[buffer_index] == 1)) {
	
	  for (i=0; i < num_files; i++) {
	    location[0] = v0;
	    location[1] = v1;
	    location[2] = v2;
	    //SET_VECTOR_ELT(buffer, i, full_buffer[i][index]);
	    //result = miget_real_value(hvol[i], location, 3, &xbuffer[i]);
	    xbuffer[i] = full_buffer[i][buffer_index];
	    
	    //Rprintf("V%i: %f\n", i, full_buffer[i][index]);

	  }
	  /* install the variable "x" into environment */
	  defineVar(install("x"), buffer, rho);
	  //SETCADDR(R_fcall, buffer);
	  //SET_VECTOR_ELT(output, index, eval(R_fcall, rho));
	  //SET_VECTOR_ELT(output, index, test);
	  /* evaluate the function */
	  xoutput[output_index] = REAL(eval(fn, rho))[0]; 
	}
	else {
	  xoutput[output_index] = 0;
	}
      }
    }
  }
  Rprintf("\nDone\n");

  /* free memory */
  for (i=0; i<num_files; i++) {
    miclose_volume(hvol[i]);
    free(full_buffer[i]);
  }
  free(full_buffer);
  UNPROTECT(2);

  /* return the results */
  return(output);
}

/* writes a hyperslab to the output filename, creating the output voluem
   to be like the second filename passed in */
void write_minc2_volume(char **output, char **like_filename,
			int *start, int *count, double *max_range,
			double *min_range, double *slab) {
  mihandle_t hvol_like, hvol_new;
  midimhandle_t dimensions_like[3], dimensions_new[3];
  unsigned long tmp_count[3];
  unsigned long tmp_start[3];
  mivolumeprops_t props;
  int i;

  /* read the like volume */
  if (miopen_volume(like_filename[0], MI2_OPEN_READ, &hvol_like) < 0 ) {
    error("Error opening volume: %s\n", like_filename[0]);
  }
  
  /* get dimensions */
  miget_volume_dimensions( hvol_like, MI_DIMCLASS_SPATIAL, MI_DIMATTR_ALL,
			   MI_DIMORDER_FILE, 3, dimensions_like );

  /* copy the dimensions to the new file */
  for (i=0; i < 3; i++) {
    micopy_dimension(dimensions_like[i], &dimensions_new[i]);
  }

  /* create the new volume */
  if ( micreate_volume(output[0], 3, dimensions_new, MI_TYPE_USHORT,
		       MI_CLASS_REAL, NULL, &hvol_new) < 0 ) {
    error("Error creating volume %s\n", output[0]);
  }
  if (micreate_volume_image(hvol_new) < 0) {
    error("Error creating volume image\n");
  }
  
  /* set valid and real range */
  miset_volume_valid_range(hvol_new, 65535, 0);
  miset_volume_range(hvol_new, max_range[0], min_range[0]);

  Rprintf("Range: %f %f\n", max_range[0], min_range[0]);

  /* write the buffer */
  for (i=0; i < 3; i++) {
    tmp_start[i] = (unsigned long) start[i];
    tmp_count[i] = (unsigned long) count[i];
  }
  if (miset_real_value_hyperslab(hvol_new, MI_TYPE_DOUBLE, 
				 (unsigned long *) tmp_start, 
				 (unsigned long *) tmp_count,
				 slab) < 0) {
    error("Error writing buffer to volume\n");
  }
  miclose_volume(hvol_like);
  miclose_volume(hvol_new);
  return;
}

