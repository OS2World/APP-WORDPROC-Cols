/*************************************************************************\
 * Program to format ASCII text in several columns.                   
 * This program is also useable to break text with long lines.
 * You can type
 * 	cols -h
 * to get a description of the valid command line parameters.
 *
 * (c) 10.92 by Ralf Seidel
 *          Wuelfrahter Str. 45
 *          42105 Wuppertal
 *          email: seidel3@wrcs3.uni-wuppertal.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Set tabs to 3 to get a readable source.
\*************************************************************************/
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <getopt.h>

#ifndef __GNUC__
#define __inline__
#define __atribute__( dummy )
#define __const__
#endif

/*
#define DEBUG
*/
/*************************************************************************\
 * Type definitions
\*************************************************************************/
typedef unsigned char uchar;

/*************************************************************************\
 * Prototypes of defined functions 
\*************************************************************************/
void usage() __attribute__(( noreturn ));
void wrong_parameter() __attribute__(( noreturn ));
void help() __attribute__(( noreturn ));
void spc( uchar* string, __const__ size_t n );
void printpg();
void printpgrest();
void puttooutbuf( uchar *string );
void setnewline();

/*************************************************************************\
 * Global variables used in this program 
\*************************************************************************/
#define DEFAULT_MARGIN 4
static uchar *prog = NULL;				/* Name of this program found in argv[0] */
/* Variables which represent the command line options initialised with
 * default values */
static int pg_lines = 66;				/* Number of lines per page */
static int pg_width = 72;				/* Number of characters per output line: */
static int col_width = 72;          /* characters per column:
                                     * maximal length of displayed text in one
                                     * column */
static int cols = 1;                /* number of text columns */
static int sepfiles = 0;            /* flag: start new files on new page? */
static int word_wrap = 0;				/* flag: word wrap? */
static int sendff = 0;              /* flag: send chr(12) afer each page? */
static int expand_tabs = 0;			/* flag: expand tabs to spaces? */
static int dbllf = 0;					/* flag: double newline characters? */
static int tab_spc = 4;					/* number of spaces to insert for one tab */
static int left_spc = 0;				/* number of blanks at the beginning of */
                                    /* each line */
static int mid_spc = 1;					/* number of blanks between columns */

/* Global variables which are used during file processing */                                    
static FILE *out_file = NULL;

static int cur_col;                 /* current text column in access */
static int cur_col_pos;					/* next postion to write characters */
static int cur_line;                /* current text line  */
static uchar *pcur_pos;					/* character arrays */
static uchar **cur_page; 				/* pointer to the page to be printed 
												 * next */

#define USAGE "%s [-BdfhW -cn -ln -mn -ofile -tn -wn -Wn files]\n"

/*************************************************************************\
 * Fill a string with n blanks and append a NULL character 
\*************************************************************************/
void spc( uchar* string, __const__ size_t n )
{
   memset( string, (int)' ', n );
   string[n] = '\0';
   return;
}

/*************************************************************************\
 * Print help lines and exit program.
\*************************************************************************/
void help()
{
	fprintf( stderr,
		USAGE 
		"Format text in several columns\n" 
		"\t-c: number of columns (%d)\n" 
		"\t-d: double every new line character found in input\n" 
		"\t-f: send formfeed (chr 12) after each page of output\n" 
		"\t-h: show this help\n" 
		"\t-l: lines per page (%d)\n" 
		"\t-m: set left left margin to n (%d)\n"
		"\t-o: name of output file (stdout)\n" 
		"\t-t: expand tabs to blanks - n = tab_spc (%d)\n" 
		"\t-w: width of one column (%d)\n" 
		"\t-W: width of output page (%d)\n" 
		"\t-s: seperate files - each file will begin on a new page\n" 
		"\t-B: break lines between words only (word wrap)\n" 
		"\nIf no file is specified stdin is used for input and stdout for output.\n" 
		"Values in brackets are the defaults\n", 
		prog, cols, pg_lines, DEFAULT_MARGIN, tab_spc, col_width, pg_width
	); /* end fprintf */
	exit(0);
}

/*************************************************************************\
 * Print error message and exit if an unknown parameter is found.
\*************************************************************************/
void usage()
{
    fprintf(stderr, "usage: " USAGE, prog );
    exit(1);
}

/*************************************************************************\
 * Print error message and exit if some parameters are wrong
\*************************************************************************/
void wrong_parameter()
{
	fprintf( stderr, "%s: wrong parameters found!\n", prog );
	exit(1);
}

void setnewline()
/*************************************************************************\
 * This fuction is called after proceding to a new line. It checks if the end
 * of a column is reached, if necessary does the output of a finished page and
 * updates the global variables pcur_pos and cur_col_pos.
 * Global variables changed:
 * 	pcur_pos		: Set to the start of the new line.
 * 	cur_col_pos	: = 0
 * 	cur_line		: New line number of current output page.
 * 	cur_col		: Changed if a new column starts.
 * 	cur_page		: Printed and cleared, if the currend page is full.
\*************************************************************************/
{
	int abs_pos;			/* Absolute position in the new line of cur_page */
	int len;					/* Absolute length or the new line */
	int spc_to_ins;		/* Number of blanks to insert in the new line */

	if ( cur_line >= pg_lines ) { 	/* End of column reached? */
		cur_col++;								/* Begin a new column and */
		cur_line = 0;							/* start at line 0 */
		if ( cur_col >= cols ) {			/* End of page reached? */
			printpg();								/* Print page and */
			cur_col = 0;							/* restart at column 0 */
		} /* end if */
	} /* end if */
	/* fill the space between the end of the last column and */
	/* the begining of the current column with blanks */
	len = strlen( cur_page[cur_line] );
	abs_pos = left_spc + cur_col * (col_width + mid_spc);
	spc_to_ins = abs_pos - len;
	spc( &cur_page[cur_line][len], spc_to_ins );
	cur_col_pos = 0;
	pcur_pos = &cur_page[cur_line][abs_pos];
	return;
}


/*************************************************************************\
 * Insert the string to the output buffer (cur_page)
\*************************************************************************/
void puttooutbuf( uchar *string )
{
   int n;
   size_t l;
   uchar *pstrc;			/* points to the next character in param "string"  */
   uchar *pc;				/* temporary pointer used during word wraping */
   uchar *pcol_start;	/* " */
   uchar *pwrap_str;		/* " */
   uchar c;					/* temporary character variable used during word
								 * wraping */
	uchar *tabstr;			/* inserted string for tabs if the flag "expand_tabs"
								 * is set */
	for ( pstrc = string; *pstrc != '\0'; pstrc++ ) {
		/* Examine every character in string before putting it to the */
		/* output buffer. */
		switch( *pstrc ) {
			case '\n':
				/* mark the end of the line */
				*pcur_pos = '\0';
				/* increment cur_line */
				cur_line+= dbllf + 1;
				setnewline();
				break;
			case '\t':
				if ( expand_tabs ) {
					n = tab_spc - cur_col_pos % tab_spc;
					if ( cur_col_pos + n >= col_width ) {
						*pcur_pos = '\0';
						cur_line++;
						setnewline();
					} else {
						if ( (tabstr = alloca(n + 1)) == NULL ) {
							perror( "alloca" );
							exit( 1 );
						}
						spc( tabstr, n );
						puttooutbuf( tabstr );
					}
					break;
				}
			default:
				/* copy character from parameter string to output page
				 * and increment the output position */
				*pcur_pos++ = *pstrc;
				cur_col_pos++;
				/* test, if the end of a column is reached
				 * if word wrapping is on, one more character can temporarly
				 * be written in a line because the line will be broken
				 * in front of this position */
				if ( cur_col_pos >= col_width + word_wrap ) {
					if ( word_wrap ) {
						/* calculate the beginning of the current column */
						pcol_start = &cur_page[cur_line][left_spc + cur_col * ( col_width + mid_spc )];
						/* Find the last space character in the string */
						for ( pc = pcur_pos - 1; !isspace(*pc) && pc > pcol_start; pc-- );
						if ( pc > pcol_start ) {
							/* A blank was found and pc points to its position */
							/* delete the blank and set pc to the next character */
							/* following */
							*pc++ = '\0';
							/* calculate the length of the string which has to be */
							/* wraped */
							l = (size_t)( pcur_pos - pc );
							/* save the rest of the line */
							pwrap_str = alloca( l );
							strncpy( pwrap_str, pc, l );
							cur_line++;
							setnewline();
							/* copy the reset to the next line */
							strncpy( pcur_pos, pwrap_str, l);
							pcur_pos+= l;
							cur_col_pos = l;
						} else {
							/* No blank was found in the current line: */
							c = *(--pcur_pos);
							*pcur_pos = '\0';
							cur_line++;
							setnewline();
							*pcur_pos++ = c;
							cur_col_pos = 1;
						} /* end if */
					} else { /* no word wraping: */
						*pcur_pos = '\0';
						cur_line++;
						setnewline();
					} /* end if word_wrap */
				} /* end if ( cur_col_pos >= col_width ) */
		} /* end switch */
	} /* end for */
	return;
}

/*************************************************************************\
 * Function to print a page if it is full. After printing each line is
 * cleared and the global variables cur_col and cur_line are reset to zero
 * Global variables changed:
 * 	cur_page	: Printed and cleared.
\************************************************************************/
void printpg()
{
	register int j;
	
	for (j = 0; j < pg_lines; j++) {
		fputs( cur_page[j], out_file );		/* print line j of cur_page */
		fputc( '\n', out_file );
		spc( cur_page[j], left_spc );			/* fill left margin */
	} /* end for */
	if ( sendff ) fputc( '\f', out_file );
	return;
}

/*************************************************************************\
 * Function to print the rest of a page at the end of the program.
 * This function also frees all memmory allocated by cur_page.
 * Global variables changed:
 * 	cur_page	: Printed and freed.
\************************************************************************/
void printpgrest()
{
	register int j;
	int n;

	n = cur_col > 0 ? pg_lines : cur_line;
	
	for ( j = 0; j < n; j++ ) {
		fputs( cur_page[j], out_file );		/* print line j of cur_page */
		fputc( '\n', out_file );
		free( cur_page[j] );
	} /* end for */
	for ( ; j < pg_lines; j++ )
		free( cur_page[j] );
	free( cur_page );
	if ( sendff ) fputc( '\f', out_file );
	return;
}
	

/************************************************************************\
 - - - - - - - - - - - - - - - - - - main - - - - - - - - - - - - - - - -
\************************************************************************/

int main( int argc, char *argv[] )
{
	uchar *ofname = NULL; 				/* Name of outputfile */
	FILE *in_file = NULL;				/* Handle of inputfile */
	uchar in_buf[129];		 				/* buffer to read the files */

	char *errptr;							/* pointer for return value of strtol */
	int i;									/* index used for loops */
	int c;									/* value returned by getopt */
	int pw_spec = 0;						/* flag: was the width of the page given
												 * as a parameter ? */
	int cw_spec = 0;						/* flag: was the width of a column given
												 * as a parameter ? */
	int cn_spec = 0;						/* flag: number of columns given as a
												 * parameter ? */
	int mw_spec = 0;						/* flag: left margin specified? *
												 * mw means margin width */
   
	#ifdef __EMX__  /* Use wildcard expansion with EMX-GCC (MSDOS & OS2) */
	/* Neither DOS nor OS/2 "standard" shells expand wildcards in the command-
	 * line. Using the emx port of gcc it is possible to exand these parameters
	 * with the following function.
	 * Wildcards found in any argv string are replaced by filenames found
	 * in the current directory.
	 * After calling this function argc and argv may have changed */
   _wildcard( &argc, &argv );
	#endif
	/* Get name of this program from parameter 0 and delete all preceding
	 * path information */
	prog = argv[0];
	if ( prog && strrchr( prog, '/' ) )
   	prog = strrchr( prog, '/' ) + 1;
	#if defined MSDOS || defined OS2
	if ( prog && strrchr( prog, '\\' ) )
		prog = strrchr(prog, '\\') + 1;
	#endif
   /* Get all given parameters and check if they are valid */
	while ((c = getopt( argc, argv, "c:dfhl:m::o:t::w:W:sB" )) != EOF) {
		switch (c) {
			case 'c':				/* Parameter specifies number of columns */
				cn_spec = 1;
				cols = strtol(optarg, &errptr, 0);
				if ( cols <= 0 || *errptr != '\0' ) {
					fprintf( stderr, "Invalid parameter for option -c\n" );
					exit( 1 );
				}
				#ifdef DEBUG
				fprintf( stderr, "number of columns set to %d\n", cols );
				#endif				
				break;
			case 'd':				/* Print an empty line after each line feed */
				dbllf = 1;
				#ifdef DEBUG
				fprintf( stderr, "double linefeed set on\n" );
				#endif				
				break;
			case 'f':				/* Send a form feed at the end of each page */
				sendff = 1;
				#ifdef DEBUG
				fprintf( stderr, "linefeed after every page set on\n" );
				#endif				
				break;
			case 'l':				/* Number of lines on one page */
				pg_lines = strtol( optarg, &errptr, 0);
				if ( pg_lines <= 0 || *errptr != '\0' ) {
					fprintf( stderr, "Invalid parameter for option -l\n" );
					exit( 1 );
				}
				#ifdef DEBUG
				fprintf( stderr, "page lines set to %d\n", pg_lines );
				#endif				
				break;
			case 'h':				/* give help */
				help();
				break;
			case 'm':				/*  Print a left margin */
				mw_spec = 1;
				if ( optarg != NULL ) {
					left_spc = strtol( optarg, &errptr, 0 );
					if ( left_spc < 0 || *errptr != '\0' ) {
						fprintf( stderr, "Invalid parameter for option -m\n" );
						exit( 1 );
					}
				} else {
					left_spc = DEFAULT_MARGIN;
				}
				#ifdef DEBUG
				fprintf( stderr, "left margin set to %d\n", left_spc );
				#endif				
				break;
			case 'o':				/* print output in a file */
				ofname = optarg;
				if ( ofname == NULL ) usage();
				#ifdef DEBUG
				fprintf( stderr, "name of output file set to %s\n", ofname );
				#endif				
				break;
			case 't':
				expand_tabs = 1;
				if ( optarg != NULL ) {
 					tab_spc = strtol( optarg, &errptr, 0 );
					if ( tab_spc <= 0 || *errptr != '\0' ) {
						fprintf( stderr, "Invalid parameter for option -t\n" );
						exit( 1 );
					}
					if ( tab_spc == 0 ) usage();
				} /* end if */
				#ifdef DEBUG
				fprintf( stderr, "tabs set to %d\n", tab_spc );
				#endif				
				break;
			case 'w':				/* specify the width of one column */
				cw_spec = 1;
				col_width = strtol( optarg, &errptr, 0 );
				if ( col_width <= 0 || *errptr != '\0' ) {
					fprintf( stderr, "Invalid parameter for option -w\n" );
					exit( 1 );
				}
				#ifdef DEBUG
				fprintf( stderr, "width of one column set to %d\n", col_width );
				#endif				
				break;
			case 'W':				/* width of a total page */
				pw_spec = 1;
				pg_width = strtol( optarg, &errptr, 0 );
				if ( pg_width <= 0 || *errptr != '\0' ) {
					fprintf( stderr, "Invalid parameter for option -W\n" );
					exit( 1 );
				}
				#ifdef DEBUG
				fprintf( stderr, "width of page set to %d\n", pg_width );
				#endif				
				break;
			case 's':				/* begin each file on a new page */
			   sepfiles = 1;
				#ifdef DEBUG
				fprintf( stderr, "seperate files set on\n" );
				#endif				
			   break;
			case 'B':				/* Break text between words only */
				word_wrap = 1;
				#ifdef DEBUG
				fprintf( stderr, "word wrap set on\n" );
				#endif				
				break;
			default:
				usage();
		}
	}  /* while ((c = getopt) != EOF) */
	/* assume the rest of parameters to be filesnames
	 * optind reflects the first command line argument which doesn't begin
	 * with a dash */

	if ( pw_spec ) { /* width of the page specified in the command line? */
		if( cw_spec ) { /* width of one column specified? */
			if( cn_spec ) { /* number of columns given? */
				if ( mw_spec ) { /* width of the left margin specified? */
					/* pw_spec & cw_spec & cn_spec & mw_spec */
					/* Test if everything fits */
					if ( cols * col_width + left_spc > pg_width )
						wrong_parameter();
					if ( cols > 1 )
						mid_spc = (pg_width - col_width * cols - left_spc ) / (cols - 1);
					#ifdef DEBUG
					fprintf( stderr, "space between columns set to %d\n", mid_spc);
					#endif
				} else {
					/* pw_spec & cw_spec & cn_spec & !mw_spec */
					/* Test if everything fits */
					if ( cols * col_width > pg_width ) wrong_parameter();
					/* Use as much space as possible between two columns and
					 * if there is a rest use it as a left margin */
					if ( cols > 1 )
						mid_spc = (pg_width - col_width * cols ) / (cols - 1);
					left_spc = pg_width - col_width * cols - mid_spc * (cols - 1);
					#ifdef DEBUG
					fprintf( stderr, "space between columns set to %d\n", mid_spc);
					fprintf( stderr, "left margin set to %d\n", left_spc);
					#endif
				} /* end mw_spec */
			} else { /* number of columns not given! */
				if ( mw_spec ) { /* width of the left margin specified? */
					/* pw_spec & cw_spec & !cn_spec & mw_spec */
					if ( col_width + left_spc > pg_width ) wrong_parameter();
					cols = (pg_width - left_spc) / col_width;
					if ( cols > 1 )
						mid_spc = (pg_width - left_spc - col_width * cols ) / (cols - 1);
					#ifdef DEBUG
					fprintf( stderr, "number of columns set to %d\n", cols );
					fprintf( stderr, "space between columns set to %d\n", mid_spc );
					#endif 		 
				} else {
					/* pw_spec & cw_spec & !cn_spec & !mw_spec */
					if ( col_width > pg_width ) wrong_parameter();
					cols = pg_width / col_width;
					if ( cols > 1 )
						mid_spc = (pg_width - col_width * cols ) / (cols - 1);
					left_spc = pg_width - col_width * cols - mid_spc * (cols - 1);
					#ifdef DEBUG
					fprintf( stderr, "number of columns set to %d\n", cols );
					fprintf( stderr, "space between columns set to %d\n", mid_spc);
					fprintf( stderr, "left margin set to %d\n", left_spc);
					#endif 		 
				}
			}
		} else { /* width of a column not specified! */
			if( cn_spec ) { /* number of columns given? */
				if ( mw_spec ) { /* width of the left margin specified? */
					/* pw_spec & !cw_spec & cn_spec & mw_spec */
					/* Test if everything fits - assume at least one character
					 * per column */
					if ( cols + left_spc > pg_width ) wrong_parameter();
					col_width = (pg_width - left_spc) / cols;
					if ( cols > 1 )
						mid_spc = (pg_width - col_width * cols - left_spc ) / (cols - 1);
					#ifdef DEBUG
					fprintf( stderr, "width of one column set to %d\n", col_width );
					fprintf( stderr, "space between columns set to %d\n", mid_spc );
					#endif
				} else {
					/* mw_spec & pw_spec & cn_spec & !mw_spec */
					/* Test if everything fits - assume at least one character
					 * per column */
					if ( cols > pg_width ) wrong_parameter();
					col_width = pg_width / cols;
					if ( cols > 1 )
						mid_spc = (pg_width - col_width * cols ) / (cols - 1);
					left_spc = pg_width - col_width * cols - mid_spc * (cols - 1);
					#ifdef DEBUG
					fprintf( stderr, "width of one column set to %d\n", col_width );
					fprintf( stderr, "space between columns set to %d\n", mid_spc);
					fprintf( stderr, "left margin set to %d\n", left_spc);
					#endif
				} /* end mw_spec */
			} else { /* number of columns not given! */
				/* Use default value for cols (1) */
				if ( mw_spec ) { /* width of the left margin specified? */
					/* pw_spec & !cw_spec & !cn_spec & mw_spec */
					/* Test if everything fits - assume at least one character
					 * per column */
					if ( left_spc >= pg_width ) wrong_parameter();
					col_width = (pg_width - left_spc);
					#ifdef DEBUG
					fprintf( stderr, "width of the column set to %d\n", col_width );
					#endif
				} else {
					/* cw_spec & cw_spec & cn_spec & !mw_spec */
					if ( 0 >= pg_width ) wrong_parameter();
					col_width = pg_width;
					#ifdef DEBUG
					fprintf( stderr, "width of the column set to %d\n", col_width );
					#endif
				} /* end mw_spec */
			}
		}
	} else { /* width of page not specified! */
		/* Don't mind if any parameter was specified or if the default value
		 * is used */
		pg_width = cols * col_width + (cols - 1) * mid_spc + left_spc;
		#ifdef DEBUG
		fprintf( stderr, "width of page set to %d\n", pg_width );
		#endif
	}

   /* Open output file or set output to stdout if no output name was found
	 * as a command line parameter. */
   if ( ofname == NULL )
      out_file = stdout;
   else
      if ( (out_file = fopen( ofname, "wt" )) == NULL ) {
          perror( "fopen" );
          exit( 1 );
      } /* end if ofname */
      
	/*  Allocate memory for the output page */
	if ( !(cur_page = (uchar**)malloc( pg_lines * sizeof( uchar* ) )) ) {
		perror( "malloc" );
		exit(1);
	} /* end if */
   for (i = 0; i < pg_lines; i++) {
   	/* allocate memory for every line. We need one extra byte if word
		 * wrapping is on, because of a posible overlapping last character */
      if ( !(cur_page[i] = (uchar*)malloc( (pg_width + word_wrap) * sizeof(uchar)))) {
         perror( "malloc" );
         exit(1);
      } /* end if */
      spc( cur_page[i], left_spc );
   } /* end for */
	pcur_pos = &cur_page[0][left_spc];
   /* Set input file to first filename for input or stdin if no input name
    * was specified. */
	if ( optind >= argc )
		in_file = stdin;
	else {
		if ( (in_file = fopen( argv[optind], "rt" )) == NULL ) {
			perror( "fopen" );
			exit(1);
		} /* end if */
		#ifdef DEBUG
   	fprintf( stderr, "File %s opened succesfully\n", argv[optind] );
		#endif
	}   	
	/* first loop which opens every file given as a parameter */
	do {
		optind++;
		/* second loop which reads every file until eof is reached */
		while ( fgets( in_buf, sizeof( in_buf ), in_file ) ) {
			puttooutbuf( in_buf );
		} /* end while */
		fclose( in_file );

      if ( sepfiles && !((cur_col == 0) & (cur_line == 0)) ) {
      	/* print the rest of the last file and fill buffer */
      	printpg();
			cur_col = cur_col_pos = cur_line = 0;
			pcur_pos = &cur_page[0][left_spc];
		}
      	
		if ( optind < argc ) {
			if ( (in_file = fopen( argv[optind], "rt" )) == NULL ) {
				/* The file was not found
				 * Nevertheless print the rest contents of the buffer
				 * and stop executing afterwards */
				printpgrest();
				perror( "fopen" );
				exit(1);
			} /* end if */
			#ifdef DEBUG
	   	fprintf( stderr, "file %s opened succesfully\n", argv[optind] );
			#endif
		} /* end if (optind < argc) */
	} while ( optind < argc ); /* end do while */

	/* print the rest of the output buffer and free the memory used for it */
	printpgrest();

   /* exit program */
	if ( out_file != stdout ) {
		fclose( out_file );
		printf( "%s ready\n", prog );
		printf( "Name of output file: %s\n", ofname );
	}
   return 0;
}
