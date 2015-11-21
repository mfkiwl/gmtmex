/*
 *	$Id$
 *
 *	Copyright (c) 1991-2015 by P. Wessel, W. H. F. Smith, R. Scharroo, J. Luis and F. Wobbe
 *      See LICENSE.TXT file for copying and redistribution conditions.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU Lesser General Public License as published by
 *      the Free Software Foundation; version 3 or any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU Lesser General Public License for more details.
 *
 *      Contact info: www.soest.hawaii.edu/gmt
 *--------------------------------------------------------------------*/
/* GMT convenience functions used by MATLAB/OCTAVE mex functions.
 * All code that requires knowledge about MATLAB/OCTAVE functions is
 * found here.
 */

#define STDC_FORMAT_MACROS
#define GMTMEX_LIB
#include "gmtmex.h"
#include <math.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <float.h>
#include <math.h>
#include <limits.h>

#ifndef rint
	#define rint(x) (floor((x)+0.5f)) //does not work reliable.
#endif

#if defined(WIN32)
#define strdup _strdup
#if !defined(lrint)
#	define lrint (int64_t)rint
#endif
#endif

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))	/* min macro */
#endif

enum MEX_dim {
	DIM_COL	= 0,	/* Holds the number of columns for vectors and x-nodes for matrix */
	DIM_ROW = 1};	/* Holds the number of rows for vectors and y-nodes for matrix */

/* Note: Wherever we say "MATLAB" below we mean "MATLAB of Octave" */

/* For the Mex interface we will wish to pass either filenames or mex variables via GMT command options.
 * We select a MATLAB variable by suppying $ as the file name.  The parser will then find these $
 * arguments and replace them with references to mex variables via the GMT API mechanisms.
 * This requires us to know which options in a module may accept a file name.  As an example,
 * consider surface whose -Lu|l option may take a grid.  To pass a MATLAB grid already in memory
 * we would use -Lu$ and give the grid as an argument to the module, e.g.,
 *    Z = gmt ('surface', '-R0/50/0/50 -I1 -V xyzfile -Lu$', uppergrid);
 * For each option that may take a file we need to know what kind of file and if this is input or output.
 * We encode this in a 3-character word XYZ, explained below.  Note that each module may
 * need several comma-separated XYZ words and these are returned as one string via GMT_Get_Moduleinfo.
 * The origin of these words are given by the THIS_MODULE_KEY in every module source code.
 *
 * X stands for the specific program option (e.g., L for -L, F for -F) or <,>
 *    for standard input,output (if reading tables) or command-line files (if reading grids).
 *    A hyphen (-) means there is no option for this item.
 * Y stands for data type (C = CPT, D = Dataset/Point, L = Dataset/Line,
 *    P = Dataset/Polygon, G = Grid, I = Image, T = Textset, X = PostScript, ? = type given via module option),
 * Z stands for primary inputs (I), primary output (O), secondary input (i) secondary output (o).
 *   Primary inputs and outputs need to be assigned, and if not explicitly given we will
 *   use the given left- and right-hand side arguments to supply input or accept output.
 *   Secondary inputs means they are only assigned if the option is given.
 *   A few modules with have Z = x, which means that normally these modules will produce PostScript,
 *   but if this option is given they will not (e.g., pscoast -M, so it will have >Dx, for instance).
 *
 * E.g., the surface example would have the word LGI.  The data types P|L|D|G|C|T stand for
 * P(olygons), L(ines), D(point data), G(rid), C(PT file), T(ext table). [We originally only had
 * D for datasets but perhaps the geometry needs to be passed too (if known); hence the P|L|D char]
 * In addition, the only common option that might take a file is -R which may take a grid as input.
 * We check for that in addition to the module-specific info passed via the key variable.
 *
 * The actual reading/writing will occur in gmt_api.c via the standard GMT containers.
 * The packing up GMT grids into MATLAB grid structs and vice versa happens after getting the
 * results out of the GMT API and before passing into back to MATLAB.
 *
 * All 5 GMT Resources are supported in this API, according to these rules:
 *  GMT_GRID:	Handled with a MATLAB grid structure that holds, we use GMT's native GMT_GRID for the passing.
 *		  + Basic header array of length 9 [xmin, xmax, ymin, ymax, zmin, zmax, reg, xinc, yinc]
 *		  + The 2-D grid array (single precision)
 *		  + An x-array of coordinates
 *		  + An y-array of coordinates
 * GMT_DATASET: Handled with a MATLAB matrix and we use GMT's native GMT_MATRIX for the passing.
 *		  + A 2-D matrix with rows and columns (double precision)
 * GMT_TEXTSET: Handled with a MATLAB cell array and we use GMT's native GMT_TEXTSET for the passing.
 *		  + A 1-D cell array with one text record per cell
 * GMT_PALETTE: Handled with a MATLAB structure and we use GMT's native GMT_PALETTE for the passing.
 *		  + colormap is the N*3 matrix for MATLAB colormaps
 *		  + range is a 2-element array with zmin and zmax
 *		  + alpha is a N-element array with transparencies
 */

mxClassID GMTMEX_type (void *API) {
	/* Get default export type from GMT and return equivalent MATLAB class */
	char value[8] = {""};
	GMT_Get_Default (API, "GMT_EXPORT_TYPE", value);
	if (!strncmp (value, "double", 6U)) return mxDOUBLE_CLASS;
	if (!strncmp (value, "single", 6U)) return mxSINGLE_CLASS;
	if (!strncmp (value, "long",   4U)) return  mxINT64_CLASS;
	if (!strncmp (value, "ulong",  5U)) return mxUINT64_CLASS;
	if (!strncmp (value, "int",    3U)) return  mxINT32_CLASS;
	if (!strncmp (value, "uint",   4U)) return mxUINT32_CLASS;
	if (!strncmp (value, "short",  5U)) return  mxINT16_CLASS;
	if (!strncmp (value, "ushort", 6U)) return mxUINT16_CLASS;
	if (!strncmp (value, "char",   4U)) return   mxINT8_CLASS;
	if (!strncmp (value, "uchar",  5U)) return  mxUINT8_CLASS;
	
	mexPrintf("Unable to interpret GMT_EXPORT_TYPE - Default to double\n");
	return mxDOUBLE_CLASS;
}

char *mxstrdup (const char *s) {
	/* A strdup replacement to be used in Mexs to avoid memory leaks since the MATLAB
	   memory management will take care to free the memory allocated by this function */
	char *d = mxMalloc (strlen (s) + 1);
	if (d == NULL) return NULL;
	strcpy (d,s);
	return d;
}

int GMTMEX_print_func (FILE *fp, const char *message) {
	/* Replacement for GMT's gmt_print_func.  It is being used indirectly via
	 * API->print_func.  Purpose of this is to allow MATLAB (which cannot use
	 * printf) to reset API->print_func to this function via GMT_Create_Session. */

	mexPrintf (message);
	return 0;
}

#define N_MEX_FIELDNAMES_GRID	19

void *GMTMEX_Get_Grid (void *API, struct GMT_GRID *G) {
	/* Given an incoming GMT grid G, build a MATLAB structure and assign the output components.
 	 * Note: Incoming GMT grid has standard padding while MATLAB grid has none. */

	int n;
	uint64_t row, col, gmt_ij;
	float  *f = NULL;
	double *dptr = NULL, *G_x = NULL, *G_y = NULL, *x = NULL, *y = NULL;
	mxArray *mxGrd = NULL, *mx_x = NULL, *mx_y= NULL;
	mxArray *mxProjectionRef = NULL;
	mxArray *mxHeader = NULL, *mxtmp = NULL;
	mxArray *grid_struct = NULL;
	char    *fieldnames[N_MEX_FIELDNAMES_GRID];	/* this array contains the names of the fields of the output grid structure. */

	if (!G->data)
		mexErrMsgTxt ("GMTMEX_Get_Grid: programming error, output matrix G is empty\n");

	memset (fieldnames, 0, N_MEX_FIELDNAMES_GRID*sizeof (char *));
	/* Return grids via a float (mxSINGLE_CLASS) matrix in a struct */
	/* Create a MATLAB struct to hold this grid.  First create field names for all struct components */
	fieldnames[0]  = mxstrdup ("ProjectionRefPROJ4");
	fieldnames[1]  = mxstrdup ("ProjectionRefWKT");
	fieldnames[2]  = mxstrdup ("range");
	fieldnames[3]  = mxstrdup ("inc");
	fieldnames[4]  = mxstrdup ("n_rows");
	fieldnames[5]  = mxstrdup ("n_columns");
	fieldnames[6]  = mxstrdup ("n_bands");
	fieldnames[7]  = mxstrdup ("registration");
	fieldnames[8]  = mxstrdup ("NoDataValue");
	fieldnames[9]  = mxstrdup ("title");
	fieldnames[10] = mxstrdup ("remark");
	fieldnames[11] = mxstrdup ("command");
	fieldnames[12] = mxstrdup ("DataType");
	fieldnames[13] = mxstrdup ("x");
	fieldnames[14] = mxstrdup ("y");
	fieldnames[15] = mxstrdup ("z");
	fieldnames[16] = mxstrdup ("x_units");
	fieldnames[17] = mxstrdup ("y_units");
	fieldnames[18] = mxstrdup ("z_units");
	grid_struct = mxCreateStructMatrix (1, 1, N_MEX_FIELDNAMES_GRID, (const char **)fieldnames );

	mxtmp = mxCreateString (G->header->ProjRefPROJ4);
	mxSetField (grid_struct, 0, (const char *) "ProjectionRefPROJ4", mxtmp);

	mxtmp = mxCreateString (G->header->ProjRefWKT);
	mxSetField (grid_struct, 0, (const char *) "ProjectionRefWKT", mxtmp);

	dptr = mxGetPr(mxtmp = mxCreateNumericMatrix (1, 6, mxDOUBLE_CLASS, mxREAL));
	for (n = 0; n < 4; n++) dptr[n] = G->header->wesn[n];
	dptr[4] = G->header->z_min;	dptr[5] = G->header->z_max;
	mxSetField (grid_struct, 0, "range", mxtmp);

	dptr = mxGetPr(mxtmp = mxCreateNumericMatrix (1, 2, mxDOUBLE_CLASS, mxREAL));
	for (n = 0; n < 2; n++) dptr[n] = G->header->inc[n];
	mxSetField (grid_struct, 0, "inc", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)G->header->ny);
	mxSetField (grid_struct, 0, (const char *) "n_rows", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)G->header->nx);
	mxSetField (grid_struct, 0, (const char *) "n_columns", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)G->header->n_bands);
	mxSetField (grid_struct, 0, (const char *) "n_bands", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)G->header->nan_value);
	mxSetField (grid_struct, 0, (const char *) "NoDataValue", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)G->header->registration);
	mxSetField (grid_struct, 0, (const char *) "registration", mxtmp);

	mxtmp = mxCreateString (G->header->title);
	mxSetField (grid_struct, 0, (const char *) "title", mxtmp);

	mxtmp = mxCreateString (G->header->command);
	mxSetField (grid_struct, 0, (const char *) "command", mxtmp);

	mxtmp = mxCreateString (G->header->remark);
	mxSetField (grid_struct, 0, (const char *) "remark", mxtmp);

	mxtmp = mxCreateString ("float32");
	mxSetField (grid_struct, 0, (const char *) "DataType", mxtmp);

	mxtmp = mxCreateString (G->header->x_units);
	mxSetField (grid_struct, 0, (const char *) "x_units", mxtmp);

	mxtmp = mxCreateString (G->header->y_units);
	mxSetField (grid_struct, 0, (const char *) "y_units", mxtmp);

	mxtmp = mxCreateString (G->header->z_units);
	mxSetField (grid_struct, 0, (const char *) "z_units", mxtmp);

	mxGrd = mxCreateNumericMatrix (G->header->ny, G->header->nx, mxSINGLE_CLASS, mxREAL);
	f = mxGetData (mxGrd);
	/* Load the real grd array into a float MATLAB array by transposing
           from padded GMT grd format to unpadded MATLAB format */
	for (row = 0; row < G->header->ny; row++) {
		for (col = 0; col < G->header->nx; col++) {
			gmt_ij = GMT_IJP (G->header, row, col);
			f[MEXG_IJ(G,row,col)] = G->data[gmt_ij];
		}
	}
	mxSetField (grid_struct, 0, "z", mxGrd);

	/* Also return the convenient x and y arrays */
	G_x = GMT_Get_Coord (API, GMT_IS_GRID, GMT_X, G);	/* Get array of x coordinates */
	G_y = GMT_Get_Coord (API, GMT_IS_GRID, GMT_Y, G);	/* Get array of y coordinates */
	mx_x = mxCreateNumericMatrix (1, G->header->nx, mxDOUBLE_CLASS, mxREAL);
	mx_y = mxCreateNumericMatrix (1, G->header->ny, mxDOUBLE_CLASS, mxREAL);
	x = mxGetData (mx_x);
	y = mxGetData (mx_y);
	memcpy (x, G_x, G->header->nx * sizeof (double));
	for (n = 0; n < G->header->ny; n++) y[G->header->ny-1-n] = G_y[n];	/* Must reverse the y-array */
	if (GMT_Destroy_Data (API, &G_x))
		mexPrintf("Warning: Failure to delete G_x (x coordinate vector)\n");
	if (GMT_Destroy_Data (API, &G_y))
		mexPrintf("Warning: Failure to delete G_y (y coordinate vector)\n");
	mxSetField (grid_struct, 0, "x", mx_x);
	mxSetField (grid_struct, 0, "y", mx_y);
	return (grid_struct);
}

void *GMTMEX_Get_Dataset (void *API, struct GMT_VECTOR *V) {
	/* Given an incoming GMT dataset via vectors, build a MATLAB matrix and assign values per column */
	uint64_t  start, col;
	uint64_t *ui8 = NULL;
	int64_t  *si8 = NULL;
	uint32_t *ui4 = NULL;
	int32_t  *si4 = NULL;
	uint16_t *ui2 = NULL;
	int16_t  *si2 = NULL;
	uint8_t  *uc1 = NULL;
	int8_t   *sc1 = NULL;
	double   *f8  = NULL;
	float    *f4  = NULL;
	mxClassID type = GMTMEX_type (API);	/* Get GMT's default data type */

	if (V == NULL || !V->data)
		mexErrMsgTxt ("GMTMEX_Get_Dataset: programming error, input dataset V is NULL or empty\n");

	/* Create a 2-D MATLAB matrix of correct size and type */
	mxArray *P = mxCreateNumericMatrix (V->n_rows, V->n_columns, type, mxREAL);
	
	switch (type) {	/* Handle pointers to data for the different classes */
		case mxDOUBLE_CLASS:    f8 = mxGetData  (P); break;
		case mxSINGLE_CLASS:    f4 = mxGetData  (P); break;
		case mxUINT64_CLASS:    ui8 = mxGetData (P); break;
		case mxINT64_CLASS:     si8 = mxGetData (P); break;
		case mxUINT32_CLASS:    ui4 = mxGetData (P); break;
		case mxINT32_CLASS:     si4 = mxGetData (P); break;
		case mxUINT16_CLASS:    ui2 = mxGetData (P); break;
		case mxINT16_CLASS:     si2 = mxGetData (P); break;
		case mxUINT8_CLASS:     uc1 = mxGetData (P); break;
		case mxINT8_CLASS:      sc1 = mxGetData (P); break;
		default:
			mexErrMsgTxt ("GMTMEX_Get_Dataset: Unsupported MATLAB data type in GMT dataset input.");
			break;
	}
	for (col = start = 0; col < V->n_columns; col++, start += V->n_rows) {	/* For each column */
		switch (type) {	/* Since each data type has different bits we must switch */
			case mxDOUBLE_CLASS:    memcpy (&f8[start],  V->data[col].f8,  V->n_rows * sizeof (double));    break;
			case mxSINGLE_CLASS:    memcpy (&f4[start],  V->data[col].f4,  V->n_rows * sizeof (float));     break;
			case mxUINT64_CLASS:    memcpy (&ui8[start], V->data[col].ui8, V->n_rows * sizeof (uint64_t));  break;
			case mxINT64_CLASS:     memcpy (&si8[start], V->data[col].si8, V->n_rows * sizeof (int64_t));   break;
			case mxUINT32_CLASS:    memcpy (&ui4[start], V->data[col].ui4, V->n_rows * sizeof (uint32_t));  break;
			case mxINT32_CLASS:     memcpy (&si4[start], V->data[col].si4, V->n_rows * sizeof (int32_t));   break;
			case mxUINT16_CLASS:    memcpy (&ui2[start], V->data[col].ui2, V->n_rows * sizeof (uint16_t));  break;
			case mxINT16_CLASS:     memcpy (&si2[start], V->data[col].si2, V->n_rows * sizeof (int16_t));   break;
			case mxUINT8_CLASS:     memcpy (&uc1[start], V->data[col].uc1, V->n_rows * sizeof (uint8_t));   break;
			case mxINT8_CLASS:      memcpy (&sc1[start], V->data[col].sc1, V->n_rows * sizeof (int8_t));    break;
			default: break;	/* Since we already checked this problem in the first switch */
		}
	}
	return (P);
}

void *GMTMEX_Get_Textset (void *API, struct GMT_TEXTSET *T) {
	/* Given a GMT textset T, build a MATLAB cell array and assign values */
	uint64_t seg, row, k;
	mxArray *C = NULL, *p = NULL;
	struct GMT_TEXTSEGMENT *S = NULL;
	char text[BUFSIZ] = {""};
	
	if (T == NULL || !T->table)
		mexErrMsgTxt ("GMTMEX_Get_Textset: programming error, input textset T is NULL or empty\n");
	/* Create a cell array to hold all records */
	k = T->n_records;	/* Actual number of text records */
	if (T->table[0]->n_segments > 1) k += T->table[0]->n_segments;	/* If more than one segment we must include segment headers */
	C = mxCreateCellMatrix (k, 1);
	/* There is only one table when used in the external API, but it may have many segments. */
	for (seg = k = 0; seg < T->table[0]->n_segments; seg++) {
		S = T->table[0]->segment[seg];
		if (T->table[0]->n_segments > 1) {	/* Place the segment header */
			sprintf (text, "> %s", S->header);
			p = mxCreateString (text);
			mxSetCell (C, k++, p);
		}
		for (row = 0; row <S->n_rows; row++, k++) {	/* Place all the text records */
			p = mxCreateString (S->record[row]);
			mxSetCell (C, k, p);
		}
	}
	return C;
}

#define N_MEX_FIELDNAMES_PS	2

void *GMTMEX_Get_PS (void *API, struct GMT_PS *P) {
	/* Given a GMT Postscript structure P, build a MATLAB PS structure */
	uint64_t *length;
	mxArray *C = NULLL;
	mxArray *PS_struct = NULL, *mxPS = NULL, *mxlength = NULL;
	char *fieldnames[N_MEX_FIELDNAMES_PS];	/* Array with the names of the fields of the output grid structure. */
	
	if (P == NULL || !P->data)
		mexErrMsgTxt ("GMTMEX_Get_PS: programming error, input PS struct P is NULL or data string is empty\n");

	memset (fieldnames, 0, N_MEX_FIELDNAMES_PS*sizeof (char *));
	/* Return PS with postscript and length in a struct */
	/* Create a MATLAB struct for this PS */
	fieldnames[0] = mxstrdup ("postscript");
	fieldnames[1] = mxstrdup ("length");
	PS_struct = mxCreateStructMatrix (1, 1, N_MEX_FIELDNAMES_PS, (const char **)fieldnames );

	mxPS     = mxCreateString (P->data);
	mxlength = mxCreateNumericMatrix (1, 1, mxUINT64_CLASS, mxREAL);
	length   = mxGetPr (mxalpha);
	
	length[0] = (uint64_t)P->n;	/* Set length of the PS string */
	mxSetField (PS_struct, 0, fieldnames[0], mxPS);
	mxSetField (PS_struct, 0, fieldnames[1], mxlength);

	return PS_struct;
}

#define N_MEX_FIELDNAMES_CPT	4

void *GMTMEX_Get_CPT (void *API, struct GMT_PALETTE *C) {
	/* Given a GMT CPT C, build a MATLAB structure and assign values */

	unsigned int k, j, n_colors;
	double *color = NULL, *alpha = NULL, *rangeMinMax = NULL, *range = NULL;
	mxArray *mxcolormap = NULL, *mxalpha = NULL, *mxrangeMinMax = NULL, *mxrange = NULL;
	mxArray *CPT_struct = NULL;
	char *fieldnames[N_MEX_FIELDNAMES_CPT];	/* Array with the names of the fields of the output grid structure. */

	if (!C->range)
		mexErrMsgTxt ("GMTMEX_Get_CPT: programming error, output CPT C is empty\n");

	memset (fieldnames, 0, N_MEX_FIELDNAMES_CPT*sizeof (char *));
	/* Return CPT via colormap, range, and alpha arrays in a struct */
	/* Create a MATLAB struct for this CPT */
	fieldnames[0] = mxstrdup ("colormap");
	fieldnames[1] = mxstrdup ("alpha");
	fieldnames[2] = mxstrdup ("range");
	fieldnames[3] = mxstrdup ("rangeMinMax");
	CPT_struct = mxCreateStructMatrix (1, 1, N_MEX_FIELDNAMES_CPT, (const char **)fieldnames );

	n_colors = (C->is_continuous) ? C->n_colors + 1 : C->n_colors;
	mxcolormap    = mxCreateNumericMatrix (n_colors, 3, mxDOUBLE_CLASS, mxREAL);
	color         = mxGetPr (mxcolormap);
	mxalpha       = mxCreateNumericMatrix (n_colors, 1, mxDOUBLE_CLASS, mxREAL);
	alpha         = mxGetPr (mxalpha);
	mxrangeMinMax = mxCreateNumericMatrix (2, 1, mxDOUBLE_CLASS, mxREAL);
	rangeMinMax   = mxGetPr (mxrangeMinMax);
	mxrange       = mxCreateNumericMatrix (C->n_colors, 2, mxDOUBLE_CLASS, mxREAL);
	range         = mxGetPr (mxrange);
	for (j = 0; j < C->n_colors; j++) {	/* Copy r/g/b from palette to MATLAB array */
		for (k = 0; k < 3; k++) color[j+k*n_colors] = C->range[j].rgb_low[k];
		alpha[j] = C->range[j].rgb_low[3];
		range[j] = C->range[j].z_low;
		range[j+C->n_colors] = C->range[j].z_high;
	}
	if (C->is_continuous) {	/* Add last color */
		for (k = 0; k < 3; k++) color[j+k*n_colors] = C->range[C->n_colors-1].rgb_high[k];
		alpha[j] = C->range[j].rgb_low[3];
	}
	rangeMinMax[0] = C->range[0].z_low;
	rangeMinMax[1] = C->range[C->n_colors-1].z_high;
	
	mxSetField (CPT_struct, 0, "colormap", mxcolormap);
	mxSetField (CPT_struct, 0, "alpha", mxalpha);
	mxSetField (CPT_struct, 0, "range", mxrange);
	mxSetField (CPT_struct, 0, "rangeMinMax", mxrangeMinMax);
	return (CPT_struct);
}

#define N_MEX_FIELDNAMES_IMAGE	21

void *GMTMEX_Get_Image (void *API, struct GMT_IMAGE *I) {
	int n;
	mwSize dim[3];
	uint8_t *u = NULL, *alpha = NULL;
	double *dptr = NULL, *I_x = NULL, *I_y = NULL, *x = NULL, *y = NULL;
	double *color = NULL;
	mxArray *mxImg = NULL, *mx_x = NULL, *mx_y= NULL, *mxalpha = NULL, *mxcolormap = NULL;
	mxArray *mxProjectionRef = NULL;
	mxArray *mxtmp = NULL;
	mxArray *image_struct = NULL;
	char    *fieldnames[N_MEX_FIELDNAMES_IMAGE];	/* this array contains the names of the fields of the output grid structure. */

	if (!I->data)
		mexErrMsgTxt ("GMTMEX_Get_Image: programming error, output image I is empty\n");

	memset (fieldnames, 0, N_MEX_FIELDNAMES_IMAGE*sizeof (char *));
	/* Return umage via a uint8_t (mxUINT8_CLASS) matrix in a struct */
	/* Create a MATLAB struct for this image */
	fieldnames[0]  = mxstrdup ("ProjectionRefPROJ4");
	fieldnames[1]  = mxstrdup ("ProjectionRefWKT");
	fieldnames[2]  = mxstrdup ("range");
	fieldnames[3]  = mxstrdup ("inc");
	fieldnames[4]  = mxstrdup ("n_rows");
	fieldnames[5]  = mxstrdup ("n_columns");
	fieldnames[6]  = mxstrdup ("n_bands");
	fieldnames[7]  = mxstrdup ("registration");
	fieldnames[8]  = mxstrdup ("NoDataValue");
	fieldnames[9]  = mxstrdup ("title");
	fieldnames[10] = mxstrdup ("remark");
	fieldnames[11] = mxstrdup ("command");
	fieldnames[12] = mxstrdup ("DataType");
	fieldnames[13] = mxstrdup ("x");
	fieldnames[14] = mxstrdup ("y");
	fieldnames[15] = mxstrdup ("image");
	fieldnames[16] = mxstrdup ("x_units");
	fieldnames[17] = mxstrdup ("y_units");
	fieldnames[18] = mxstrdup ("z_units");
	fieldnames[19] = mxstrdup ("colormap");
	fieldnames[20] = mxstrdup ("alpha");
	image_struct = mxCreateStructMatrix (1, 1, N_MEX_FIELDNAMES_IMAGE, (const char **)fieldnames );

	mxtmp = mxCreateString (I->header->ProjRefPROJ4);
	mxSetField (image_struct, 0, (const char *) "ProjectionRefPROJ4", mxtmp);

	mxtmp = mxCreateString (I->header->ProjRefWKT);
	mxSetField (image_struct, 0, (const char *) "ProjectionRefWKT", mxtmp);

	dptr = mxGetPr(mxtmp = mxCreateNumericMatrix (1, 6, mxDOUBLE_CLASS, mxREAL));
	for (n = 0; n < 4; n++) dptr[n] = I->header->wesn[n];
	dptr[4] = I->header->z_min;	dptr[5] = I->header->z_max;
	mxSetField (image_struct, 0, "range", mxtmp);

	dptr = mxGetPr(mxtmp = mxCreateNumericMatrix (1, 2, mxDOUBLE_CLASS, mxREAL));
	for (n = 0; n < 2; n++) dptr[n] = I->header->inc[n];
	mxSetField (image_struct, 0, "inc", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)I->header->ny);
	mxSetField (image_struct, 0, (const char *) "n_rows", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)I->header->nx);
	mxSetField (image_struct, 0, (const char *) "n_columns", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)I->header->n_bands);
	mxSetField (image_struct, 0, (const char *) "n_bands", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)I->header->registration);
	mxSetField (image_struct, 0, (const char *) "registration", mxtmp);

	mxtmp = mxCreateDoubleScalar ((double)I->header->nan_value);
	mxSetField (image_struct, 0, (const char *) "NoDataValue", mxtmp);
	
	dptr = mxGetPr(mxtmp = mxCreateNumericMatrix (1, 2, mxDOUBLE_CLASS, mxREAL));
	dptr[0] = I->header->ny;	dptr[1] = I->header->nx;
	mxSetField (image_struct, 0, "dim", mxtmp);

	mxtmp = mxCreateString (I->header->title);
	mxSetField (image_struct, 0, (const char *) "title", mxtmp);

	mxtmp = mxCreateString (I->header->command);
	mxSetField (image_struct, 0, (const char *) "command", mxtmp);

	mxtmp = mxCreateString (I->header->remark);
	mxSetField (image_struct, 0, (const char *) "remark", mxtmp);

	mxtmp = mxCreateString ("uint8");
	mxSetField (image_struct, 0, (const char *) "DataType", mxtmp);

	mxtmp = mxCreateString (I->header->x_units);
	mxSetField (image_struct, 0, (const char *) "x_units", mxtmp);

	mxtmp = mxCreateString (I->header->y_units);
	mxSetField (image_struct, 0, (const char *) "y_units", mxtmp);

	mxtmp = mxCreateString (I->header->z_units);
	mxSetField (image_struct, 0, (const char *) "z_units", mxtmp);

	if (I->ColorMap != NULL) {	/* Indexed image has a color map */
		mxcolormap = mxCreateNumericMatrix (I->nIndexedColors, 3, mxDOUBLE_CLASS, mxREAL);
		mxImg = mxCreateNumericMatrix (I->header->ny, I->header->nx, mxUINT8_CLASS, mxREAL);
		color = mxGetPr (mxcolormap);
		u = mxGetData (mxImg);
		for (n = 0; n < 4 * I->nIndexedColors && I->ColorMap[n] >= 0; n++) color[n] = (uint8_t)I->ColorMap[n];
		n /= 4;
		memcpy (u, I->data, I->header->nm * sizeof (uint8_t));
		mxSetField (image_struct, 0, "colormap", mxcolormap);
	}	
	else if (I->header->n_bands == 1) { /* gray image */
		mxImg = mxCreateNumericMatrix (I->header->ny, I->header->nx, mxUINT8_CLASS, mxREAL);
		u = mxGetData (mxImg);
		memcpy (u, I->data, I->header->nm * sizeof (uint8_t));
	}
	else if (I->header->n_bands == 3) { /* RGB image */
		dim[0] = I->header->ny;	dim[1] = I->header->nx; dim[2] = 3;
		mxImg = mxCreateNumericArray (3, dim, mxUINT8_CLASS, mxREAL);
		u = mxGetData (mxImg);
		/*
		for (n = 0; n < I->header->nm; n++)
			for (k = 0; k < 3; k++)
				u[n+k*I->header->nm] = (uint8_t)I->data[3*n+k];
		*/
		memcpy (u, I->data, 3 * I->header->nm * sizeof (uint8_t));
	}
	else if (I->header->n_bands == 4) { /* RGBA image, with a color map */
		dim[0] = I->header->ny;	dim[1] = I->header->nx; dim[2] = 3;
		mxImg = mxCreateNumericArray (3, dim, mxUINT8_CLASS, mxREAL);
		u = mxGetData (mxImg);
		mxalpha = mxCreateNumericMatrix (I->header->ny, I->header->nx, mxUINT8_CLASS, mxREAL);
		alpha = mxGetData (mxalpha);
		memcpy(u, I->data, 3 * I->header->nm * sizeof (uint8_t)); 
		memcpy(alpha, &(I->data)[3 * I->header->nm], I->header->nm * sizeof (uint8_t)); 
		/*
		for (n = 0; n < I->header->nm; n++) {
			for (k = 0; k < 3; k++)
				u[n+k*I->header->nm] = (uint8_t)I->data[4*n+k];
			alpha[n] = (uint8_t)I->data[4*n+3];
		}
		*/
		mxSetField (image_struct, 0, "alpha", mxalpha);
	}
	mxSetField (image_struct, 0, "image", mxImg);

	/* Also return the convenient x and y arrays */
	I_x = GMT_Get_Coord (API, GMT_IS_IMAGE, GMT_X, I);	/* Get array of x coordinates */
	I_y = GMT_Get_Coord (API, GMT_IS_IMAGE, GMT_Y, I);	/* Get array of y coordinates */
	mx_x = mxCreateNumericMatrix (1, I->header->nx, mxDOUBLE_CLASS, mxREAL);
	mx_y = mxCreateNumericMatrix (1, I->header->ny, mxDOUBLE_CLASS, mxREAL);
	x = mxGetData (mx_x);
	y = mxGetData (mx_y);
	memcpy (x, I_x, I->header->nx * sizeof (double));
	for (n = 0; n < I->header->ny; n++) y[I->header->ny-1-n] = I_y[n];	/* Must reverse the y-array */
	if (GMT_Destroy_Data (API, &I_x))
		mexPrintf("Warning: Failure to delete I_x (x coordinate vector)\n");
	if (GMT_Destroy_Data (API, &I_y))
		mexPrintf("Warning: Failure to delete I_y (y coordinate vector)\n");
	mxSetField (image_struct, 0, "x", mx_x);
	mxSetField (image_struct, 0, "y", mx_y);
	return (image_struct);
}

struct GMT_GRID *GMTMEX_grid_init (void *API, unsigned int direction, unsigned int module_input, const mxArray *ptr) {
	/* Used to Create an empty Grid container to hold a GMT grid.
 	 * If direction is GMT_IN then we are given a MATLAB grid and can determine its size, etc.
	 * If direction is GMT_OUT then we allocate an empty GMT grid as a destination. */
	unsigned int row, col;
	uint64_t gmt_ij;
	struct GMT_GRID *G = NULL;

	if (direction == GMT_IN) {	/* Dimensions are known from the input pointer */
		unsigned int registration;
		unsigned int family = (module_input) ? GMT_IS_GRID|GMT_VIA_MODULE_INPUT : GMT_IS_GRID;
		mxArray *mx_ptr = NULL, *mxGrid = NULL, *mxHdr = NULL;

		if (mxIsEmpty (ptr))
			mexErrMsgTxt ("GMTMEX_grid_init: The input that was supposed to contain the Grid, is empty\n");
		if (!mxIsStruct (ptr)) {
			if (!mxIsCell (ptr))
				mexErrMsgTxt ("GMTMEX_grid_init: Expected a Grid structure or Cell array for input\n");
			else {		/* Test that we have a {MxN,1x9} cell array */
				if (mxGetM(ptr) != 2 && mxGetN(ptr) != 2)
					mexErrMsgTxt ("GMTMEX_grid_init: Cell array must contain two elements\n");
				else {
					mxGrid = mxGetCell(ptr, 0);
					mxHdr  = mxGetCell(ptr, 1);
					if (mxGetM(mxGrid) < 2 || mxGetN(mxGrid) < 2)
						mexErrMsgTxt ("GMTMEX_grid_init: First element of grid's cell array must contain a decent matrix\n");
					if (mxGetM(mxHdr) != 1 || mxGetN(mxHdr) != 9)
						mexErrMsgTxt ("GMTMEX_grid_init: grid's cell array second element must contain a 1x9 vector\n");
					if (!mxIsSingle(mxGrid) && !mxIsDouble(mxGrid))
						mexErrMsgTxt ("GMTMEX_grid_init: grid's cell matrix must be either single or double.\n");
				}
			}
		}

		if (mxIsStruct(ptr)) {	/* Passed a regular MEX Grid structure */
			double *inc = NULL, *range = NULL, *reg = NULL;
			mx_ptr = mxGetField (ptr, 0, "inc");
			if (mx_ptr == NULL)
				mexErrMsgTxt ("GMTMEX_grid_init: Could not find inc array with Grid increments\n");
			inc = mxGetData (mx_ptr);

			mx_ptr = mxGetField (ptr, 0, "range");
			if (mx_ptr == NULL)
				mexErrMsgTxt ("GMTMEX_grid_init: Could not find range array for Grid range\n");
			range = mxGetData (mx_ptr);

			mx_ptr = mxGetField (ptr, 0, "registration");
			if (mx_ptr == NULL)
				mexErrMsgTxt ("GMTMEX_grid_init: Could not find registration array for Grid registration\n");

			mxGrid = mxGetField(ptr, 0, "z");
			if (mxGrid == NULL)
				mexErrMsgTxt ("GMTMEX_grid_init: Could not find data array for Grid\n");
			if (!mxIsSingle(mxGrid) && !mxIsDouble(mxGrid))
				mexErrMsgTxt ("GMTMEX_grid_init: data array must be either single or double.\n");

			reg = mxGetData (mx_ptr);
			registration = (unsigned int)lrint(reg[0]);
			if ((G = GMT_Create_Data (API, family, GMT_IS_SURFACE, GMT_GRID_ALL,
			                          NULL, range, inc, registration, GMT_NOTSET, NULL)) == NULL)
				mexErrMsgTxt ("GMTMEX_grid_init: Failure to alloc GMT source matrix for input\n");

			G->header->z_min = range[4];
			G->header->z_max = range[5];
		}
		else {	/* Passed header and grid separately */
			double *h = mxGetData(mxHdr);
			registration = (unsigned int)lrint(h[6]);
			if ((G = GMT_Create_Data (API, family, GMT_IS_SURFACE, GMT_GRID_ALL,
			                          NULL, h, &h[7], registration, GMT_NOTSET, NULL)) == NULL)
				mexErrMsgTxt ("GMTMEX_grid_init: Failure to alloc GMT source matrix for input\n");
			G->header->z_min = h[4];
			G->header->z_max = h[5];
		}

		if (mxIsSingle(mxGrid)) {
			float *f4 = mxGetData(mxGrid);
			for (row = 0; row < G->header->ny; row++) {
				for (col = 0; col < G->header->nx; col++) {
					gmt_ij = GMT_IJP (G->header, row, col);
					G->data[gmt_ij] = f4[MEXG_IJ(G,row,col)];
				}
			}
		}
		else {
			double *f8 = mxGetData(mxGrid);
			for (row = 0; row < G->header->ny; row++) {
				for (col = 0; col < G->header->nx; col++) {
					gmt_ij = GMT_IJP (G->header, row, col);
					G->data[gmt_ij] = (float)f8[MEXG_IJ(G,row,col)];
				}
			}
		}
		GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_grid_init: Allocated GMT Grid %lx\n", (long)G);
		GMT_Report (API, GMT_MSG_DEBUG,
		            "GMTMEX_grid_init: Registered GMT Grid array %lx via memory reference from MATLAB\n",
		            (long)G->data);
	}
	else {	/* Just allocate an empty container to hold an output grid (signal this by passing NULLs) */
		if ((G = GMT_Create_Data (API, GMT_IS_GRID, GMT_IS_SURFACE, GMT_GRID_HEADER_ONLY,
		                          NULL, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_grid_init: Failure to alloc GMT blank grid container for holding output grid\n");
	}
	return (G);
}

struct GMT_IMAGE *GMTMEX_image_init (void *API, unsigned int direction, unsigned int module_input, const mxArray *ptr) {
	/* Used to Create an empty Image container to hold a GMT image.
 	 * If direction is GMT_IN then we are given a MATLAB image and can determine its size, etc.
	 * If direction is GMT_OUT then we allocate an empty GMT image as a destination. */
	unsigned int row, col;
	uint64_t gmt_ij;
	struct GMT_IMAGE *I = NULL;
	if (direction == GMT_IN) {	/* Dimensions are known from the input pointer */
		unsigned int family = (module_input) ? GMT_IS_IMAGE|GMT_VIA_MODULE_INPUT : GMT_IS_IMAGE;
		mxArray *mx_ptr = NULL;
		double *inc = NULL, *range = NULL;
		float *f = NULL;

		if (mxIsEmpty (ptr))
			mexErrMsgTxt ("GMTMEX_image_init: The input that was supposed to contain the Image, is empty\n");

		if (!mxIsStruct (ptr))
			mexErrMsgTxt ("GMTMEX_image_init: Expected a Image structure for input\n");

		mx_ptr = mxGetField (ptr, 0, "range");
		if (mx_ptr == NULL)
			mexErrMsgTxt ("GMTMEX_image_init: Could not find range array for Image range\n");
		range = mxGetData (mx_ptr);

		mx_ptr = mxGetField (ptr, 0, "inc");
		if (mx_ptr == NULL)
			mexErrMsgTxt ("GMTMEX_image_init: Could not find inc array with Image increments\n");
		inc = mxGetData (mx_ptr);

		if ((I = GMT_Create_Data (API, family, GMT_IS_SURFACE, GMT_GRID_ALL,
			NULL, range, inc, 0, GMT_NOTSET, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_image_init: Failure to alloc GMT source image for input\n");

		I->header->z_min = range[4];
		I->header->z_max = range[5];

		mx_ptr = mxGetField (ptr, 0, "z");
		if (mx_ptr == NULL)
			mexErrMsgTxt ("GMTMEX_image_init: Could not find data array for Grid\n");

		f = mxGetData (mx_ptr);
		for (row = 0; row < I->header->ny; row++) {
			for (col = 0; col < I->header->nx; col++, gmt_ij++) {
				gmt_ij = GMT_IJP (I->header, row, col);
				I->data [gmt_ij] = f[MEXG_IJ(I,row,col)];
			}
		}
		GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_image_init: Allocated GMT Image %lx\n", (long)I);
		GMT_Report (API, GMT_MSG_DEBUG,
		            "GMTMEX_image_init: Registered GMT Image array %lx via memory reference from MATLAB\n",
		            (long)I->data);
	}
	else {	/* Just allocate an empty container to hold an output grid (signal this by passing NULLs) */
		if ((I = GMT_Create_Data (API, GMT_IS_IMAGE, GMT_IS_SURFACE, GMT_GRID_HEADER_ONLY,
		                          NULL, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_image_init: Failure to alloc GMT blank image container for holding output image\n");
		GMT_Set_Default (API, "API_IMAGE_LAYOUT", "TCLS");	/* State how we wish to receive images from GDAL */
	}
	return (I);
}

void *GMTMEX_dataset_init (void *API, unsigned int direction, unsigned int module_input, const mxArray *ptr) {
	/* Used to create containers to hold or receive data:
	 * direction == GMT_IN:  Create empty Matrix container, associate it with mex data matrix, and use as GMT input.
	 * direction == GMT_OUT: Create empty Vector container, let GMT fill it out, and use for Mex output.
 	 * Note that in GMT these will be considered DATASETs via GMT_MATRIX or GMT_VECTOR.
 	 * If direction is GMT_IN then we are given a MATLAB matrix and can determine size, etc.
	 * If output then we dont know size so all we do is specify data type. */
	if (direction == GMT_IN) {	/* Dimensions are known, extract them and set dim array for a GMT_MATRIX resource */
		uint64_t dim[3] = {0, 0, 0};
		struct GMT_MATRIX *M = NULL;
		unsigned int family = (module_input) ? GMT_IS_MATRIX|GMT_VIA_MODULE_INPUT : GMT_IS_MATRIX;
		mxClassID type;
		if (!ptr) mexErrMsgTxt("GMTMEX_dataset_init: input is empty where it can't be.\n");
		type  = mxGetClassID(ptr);
		if (!mxIsNumeric (ptr)) mexErrMsgTxt ("GMTMEX_dataset_init: Expected a Matrix for input\n");
		dim[DIM_ROW] = mxGetM (ptr);	/* Number of rows */
		dim[DIM_COL] = mxGetN (ptr);	/* Number of columns */
		if ((M = GMT_Create_Data (API, family, GMT_IS_PLP, 0, dim, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_dataset_init: Failure to alloc GMT source matrix\n");

		GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_dataset_init: Allocated GMT Matrix %lx\n", (long)M);
		M->n_rows = dim[DIM_ROW];	M->n_columns = dim[DIM_COL];
		switch (type) {	/* Assign pointer to corresponding GMT matrix union pointer */
			case mxDOUBLE_CLASS: M->type = GMT_DOUBLE; M->data.f8  =             mxGetData (ptr); break;
			case mxSINGLE_CLASS: M->type = GMT_FLOAT;  M->data.f4  =    (float *)mxGetData (ptr); break;
			case mxUINT64_CLASS: M->type = GMT_ULONG;  M->data.ui8 = (uint64_t *)mxGetData (ptr); break;
			case mxINT64_CLASS:  M->type = GMT_LONG;   M->data.si8 =  (int64_t *)mxGetData (ptr); break;
			case mxUINT32_CLASS: M->type = GMT_UINT;   M->data.ui4 = (uint32_t *)mxGetData (ptr); break;
			case mxINT32_CLASS:  M->type = GMT_INT;    M->data.si4 =  (int32_t *)mxGetData (ptr); break;
			case mxUINT16_CLASS: M->type = GMT_USHORT; M->data.ui2 = (uint16_t *)mxGetData (ptr); break;
			case mxINT16_CLASS:  M->type = GMT_SHORT;  M->data.si2 =  (int16_t *)mxGetData (ptr); break;
			case mxUINT8_CLASS:  M->type = GMT_UCHAR;  M->data.uc1 =  (uint8_t *)mxGetData (ptr); break;
			case mxINT8_CLASS:   M->type = GMT_CHAR;   M->data.sc1 =   (int8_t *)mxGetData (ptr); break;
			default:
				mexErrMsgTxt ("GMTMEX_dataset_init: Unsupported MATLAB data type in GMT matrix input.");
				break;
		}
		/* Data from MATLAB and Octave(mex) is in col format and data from Octave(oct) is in row format */
#ifdef GMT_OCTOCT
		M->dim = M->n_columns;
#else
		M->dim = M->n_rows;
#endif
		M->alloc_mode = GMT_ALLOC_EXTERNALLY;	/* Since matrix was allocated by MATLAB/Octave */
		M->shape = MEX_COL_ORDER;		/* Either col or row order, depending on MATLAB/Octave setting in gmtmex.h */
		return (M);
	}
	else {	/* To receive data from GMT we use a GMT_VECTOR resource instead */
		struct GMT_VECTOR *V = NULL;
		/* There are no dimensions and we are just getting an empty container for output */
		if ((V = GMT_Create_Data (API, GMT_IS_VECTOR, GMT_IS_PLP, 0, NULL, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_dataset_init: Failure to alloc GMT source vector\n");
		GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_dataset_init: Allocated GMT Vector %lx\n", (long)V);
		return (V);
	}
}

struct GMT_PALETTE *GMTMEX_cpt_init (void *API, unsigned int direction, unsigned int module_input, const mxArray *ptr) {
	/* Used to Create an empty CPT container to hold a GMT CPT.
 	 * If direction is GMT_IN then we are given a MATLAB CPT and can determine its size, etc.
	 * If direction is GMT_OUT then we allocate an empty GMT CPT as a destination. */
	struct GMT_PALETTE *P = NULL;
	if (direction == GMT_IN) {	/* Dimensions are known from the input pointer */
		unsigned int k, j, one = 1;
		uint64_t dim[2];
		unsigned int family = (module_input) ? GMT_IS_CPT|GMT_VIA_MODULE_INPUT : GMT_IS_CPT;
		mxArray *mx_ptr = NULL;
		double dz, *colormap = NULL, *range = NULL, *rangeMinMax = NULL, *alpha = NULL;

		if (mxIsEmpty (ptr))
			mexErrMsgTxt ("GMTMEX_cpt_init: The input that was supposed to contain the CPT, is empty\n");
		if (!mxIsStruct (ptr))
			mexErrMsgTxt ("GMTMEX_cpt_init: Expected a CPT structure for input\n");
		mx_ptr = mxGetField (ptr, 0, "colormap");
		if (mx_ptr == NULL)
			mexErrMsgTxt ("GMTMEX_cpt_init: Could not find colormap array with CPT values\n");
		dim[0] = mxGetM (mx_ptr);	/* Number of rows in colormap */
		if (dim[0] < 1)
			mexErrMsgTxt ("GMTMEX_cpt_init: Colormap array has no CPT values\n");
		colormap = mxGetData (mx_ptr);

		mx_ptr = mxGetField (ptr, 0, "range");
		if (mx_ptr == NULL) {	/* OK, we don't have the 'range' member but than we must have the 'rangeMinMax' */
			mx_ptr = mxGetField(ptr, 0, "rangeMinMax");
			if (mx_ptr == NULL) {
				mexErrMsgTxt("GMTMEX_cpt_init: Could not find neither the 'range' nor the 'rangeMinMax' arrays for CPT range\n");
			}
			rangeMinMax = mxGetData(mx_ptr);
			dim[1] = dim[0];	/* This means discrete CPT */
		}
		else {	/* With range we can determine if continuous or discrete */
			range  = mxGetData(mx_ptr);
			dim[1] = mxGetM (mx_ptr);	/* Length of range array */
		}
		if (dim[0] > dim[1]) {	/* This only happens when we have a continuous color table */
			dim[1] = dim[0];	/* Actual length of colormap array */
			dim[0]--;		/* Number of CPT slices */
		}
		else	/* Discrete, so the one offset needs to be zero */
			one = 0;
		mx_ptr = mxGetField (ptr, 0, "alpha");
		if (mx_ptr == NULL)
			mexErrMsgTxt ("GMTMEX_cpt_init: Could not find alpha array for CPT transparency\n");
		alpha = mxGetData (mx_ptr);

		if ((P = GMT_Create_Data (API, family, GMT_IS_NONE, 0, dim, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_cpt_init: Failure to alloc GMT source CPT for input\n");

		if (rangeMinMax)	/* Means we didn't get the full range array and need to compute it */
			dz = (range[1] - range[0]) / P->n_colors;

		for (j = 0; j < P->n_colors; j++) {	/* OK to access j+1'th elemenent since length of colormap is P->n_colors+1 */
			for (k = 0; k < 3; k++) {
				P->range[j].rgb_low[k]  = colormap[j+k*dim[1]];
				P->range[j].rgb_high[k] = colormap[(j+one)+k*dim[1]];
			}
			P->range[j].rgb_low[3]  = alpha[j];
			P->range[j].rgb_high[3] = alpha[j+one];
			if (range) {
				P->range[j].z_low  = range[j];
				P->range[j].z_high = range[j+P->n_colors];
			}
			else {
				P->range[j].z_low = range[0] + j * dz;
				P->range[j].z_high = P->range[j].z_low + dz;
			}
			P->range[j].annot = 3;	/* Enforce annotations for now */
		}
		P->is_continuous = one;
		GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_cpt_init: Allocated GMT CPT %lx\n", (long)P);
	}
	else {	/* Just allocate an empty container to hold an output grid (signal this by passing NULLs) */
		if ((P = GMT_Create_Data (API, GMT_IS_CPT, GMT_IS_NONE, 0,
                        NULL, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_cpt_init: Failure to alloc GMT blank CPT container for holding output CPT\n");
	}
	return (P);
}

struct GMT_TEXTSET *GMTMEX_text_init (void *API, unsigned int direction, unsigned int module_input, unsigned int family, const mxArray *ptr) {
	/* Used to Create an empty Textset container to hold a GMT TEXTSET.
 	 * If direction is GMT_IN then we are given a MATLAB cell array and can determine its size, etc.
	 * If direction is GMT_OUT then we allocate an empty GMT TEXTSET as a destination. */
	struct GMT_TEXTSET *T = NULL;
	if (direction == GMT_IN) {	/* Dimensions are known from the MATLAB input pointer */
		uint64_t rec, dim[3] = {1, 1, 0};
		if (module_input) family |= GMT_VIA_MODULE_INPUT;
		unsigned int got_text = 0;
		mxArray *mx_ptr = NULL;
		char *txt = NULL;
		struct GMT_TEXTSEGMENT *S = NULL;	/* Shorthand for current segment */

		if (mxIsEmpty (ptr))
			mexErrMsgTxt ("GMTMEX_text_init: The input that was supposed to contain the Cell array is empty\n");
		dim[GMT_ROW] = mxGetM (ptr);	/* Number of records */
		if (mxIsChar (ptr) && dim[GMT_ROW] == 1)	/* Special case: Got a single text record */
			got_text = 1;
		else if (!mxIsCell (ptr))
			mexErrMsgTxt ("GMTMEX_text_init: Expected a Cell array for input\n");
		if (dim[GMT_ROW] == 1 && !got_text) {	/* Check if we got a transpose arrangement or just one record */
			rec = mxGetN (ptr);	/* Also possibly number of records */
			if (rec > 1) dim[GMT_ROW] = rec;	/* User gave row-vector of cells */
		}
		if ((T = GMT_Create_Data (API, family, GMT_IS_NONE, 0, dim, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_text_init: Failure to alloc GMT source TEXTSET for input\n");
		S = T->table[0]->segment[0];	/* Only one segment coming from MATLAB */
		S->n_rows = dim[GMT_ROW];
		T->alloc_mode = GMT_ALLOC_EXTERNALLY;
		if (got_text)	/* Just got that single record */
			S->record[0] = mxArrayToString (ptr);
		else {	/* Must get strings out of the cell array */
			for (rec = 0; rec < S->n_rows; rec++) {
				mx_ptr = mxGetCell (ptr, rec);
				txt = mxArrayToString (mx_ptr);
				S->record[rec] = txt;
			}
		}
		T->n_records = T->table[0]->n_records = S->n_rows;
		GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_text_init: Allocated GMT TEXTSET %lx\n", (long)T);
	}
	else {	/* Just allocate an empty container to hold an output grid (signal this by passing NULLs) */
		if ((T = GMT_Create_Data (API, GMT_IS_TEXTSET, GMT_IS_NONE, 0,
                        NULL, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_text_init: Failure to alloc GMT TEXTSET container for holding output TEXT\n");
	}
	return (T);
}

struct GMT_PS *GMTMEX_PS_init (void *API, unsigned int direction, unsigned int module_input, unsigned int family, const mxArray *ptr) {
	/* Used to Create an empty PS container to hold a GMT PS object.
 	 * If direction is GMT_IN then we are given a MATLAB structure with known sizes.
	 * If direction is GMT_OUT then we allocate an empty GMT PS as a destination. */
	struct GMT_PS *P = NULL;
	if (direction == GMT_IN) {	/* Dimensions are known from the MATLAB input pointer */
		uint64_t dim[1] = {0}, *length = NULL;
		if (module_input) family |= GMT_VIA_MODULE_INPUT;
		mxArray *mx_ptr = NULL;

		if (mxIsEmpty (ptr))
			mexErrMsgTxt ("GMTMEX_PS_init: The input that was supposed to contain the PS structure is empty\n");
		if (!mxIsStruct (ptr))
			mexErrMsgTxt ("GMTMEX_PS_init: Expected a MATLAB PS structure for input\n");
		mx_ptr = mxGetField (ptr, 0, "postscript");
		if (!mxIsEmpty (mx_ptr) || !mxIsChar (mx_ptr)
			mexErrMsgTxt ("GMTMEX_PS_init: Expected structure to contain a text array for PostScript\n");
		PS = mxGetData (mx_ptr);
		mx_ptr = mxGetField (ptr, 0, "length");
		if (!mxIsEmpty (mx_ptr) || !mxIsUint64 (mx_ptr)
			mexErrMsgTxt ("GMTMEX_PS_init: Expected structure to contain a coutner for PostScript length\n");
		length = mxGetData (mx_ptr);
		if (length[0] == 0)
			mexErrMsgTxt ("GMTMEX_PS_init: Dimension of PostScript given as zero\n");
		dim[0] = length[0];
		if ((P = GMT_Create_Data (API, family, GMT_IS_NONE, 0, dim, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_PS_init: Failure to alloc GMT source PS for input\n");
		P->data = PS;	/* PostScript coming from MATLAB */
		P->alloc_mode = GMT_ALLOC_EXTERNALLY;
		GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_PS_init: Allocated GMT PS %lx\n", (long)P);
	}
	else {	/* Just allocate an empty container to hold an output PS object (signal this by passing NULLs) */
		if ((P = GMT_Create_Data (API, GMT_IS_PS, GMT_IS_NONE, 0,
                        NULL, NULL, NULL, 0, 0, NULL)) == NULL)
			mexErrMsgTxt ("GMTMEX_PS_init: Failure to alloc GMT PS container for holding output PostScript\n");
	}
	return (P);
}

// (void *API, unsigned int family, unsigned int geometry, unsigned int direction, const mxArray *ptr, int *ID)

void * GMTMEX_Register_IO (void *API, struct GMT_RESOURCE *X, const mxArray *ptr) {
	/* Create the grid or matrix container, register it, and return the ID */
	void *obj = NULL;		/* Pointer to the container we created */
	char *name[2] = {"Matrix", "CellArray"};
	unsigned int module_input = (X->option->option == GMT_OPT_INFILE);
	X->object_ID = GMT_NOTSET;

	switch (X->family) {
		case GMT_IS_GRID:
			/* Get an empty grid, and if input we associate it with the MATLAB grid pointer */
			obj = GMTMEX_grid_init (API, X->direction, module_input, ptr);
			X->object_ID = GMT_Get_ID (API, GMT_IS_GRID, X->direction, obj);
			GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_Register_IO: Got Grid with Object ID %d\n", X->object_ID);
			break;
		case GMT_IS_IMAGE:
			/* Get an empty image, and if input we associate it with the MATLAB image pointer */
			obj = GMTMEX_image_init (API, X->direction, module_input, ptr);
			X->object_ID = GMT_Get_ID (API, GMT_IS_IMAGE, X->direction, obj);
			GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_Register_IO: Got Image with Object ID %d\n", X->object_ID);
			break;
		case GMT_IS_DATASET:
			/* Ostensibly a DATASET, but it might be a TEXTSET passed via a cell array, so we must check */
			if (X->direction == GMT_IN && mxIsCell (ptr))	/* Got TEXTSET input */
				obj = GMTMEX_text_init (API, X->direction, module_input, GMT_IS_TEXTSET, ptr);
			else	/* Get a matrix container, and if input we associate it with the MATLAB pointer */
				obj = GMTMEX_dataset_init (API, X->direction, module_input, ptr);
			X->object_ID = GMT_Get_ID (API, X->family, X->direction, obj);
			GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_Register_IO: Got %s with Object ID %d\n", name[X->family], X->object_ID);
			break;
		case GMT_IS_TEXTSET:
			/* Get a TEXTSET container, and if input we associate it with the MATLAB pointer */
			obj = GMTMEX_text_init (API, X->direction, module_input, GMT_IS_TEXTSET, ptr);
			X->object_ID = GMT_Get_ID (API, GMT_IS_TEXTSET, X->direction, obj);
			GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_Register_IO: Got TEXTSET with Object ID %d\n", X->object_ID);
			break;
		case GMT_IS_CPT:
			/* Get a CPT container, and if input we associate it with the MATLAB CPT pointer */
			obj = GMTMEX_cpt_init (API, X->direction, module_input, ptr);
			X->object_ID = GMT_Get_ID (API, GMT_IS_CPT, X->direction, obj);
			GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_Register_IO: Got CPT with Object ID %d\n", X->object_ID);
			break;
		case GMT_IS_PS:
			/* Get a PS container, and if input we associate it with the MATLAB PS pointer */
			obj = GMTMEX_PS_init (API, X->direction, module_input, ptr);
			X->object_ID = GMT_Get_ID (API, GMT_IS_PS, X->direction, obj);
			GMT_Report (API, GMT_MSG_DEBUG, "GMTMEX_Register_IO: Got PS with Object ID %d\n", X->object_ID);
			break;
		default:
			GMT_Report (API, GMT_MSG_NORMAL, "GMTMEX_Register_IO: Bad data type (%d)\n", X->family);
			break;
	}
	return (obj);
}