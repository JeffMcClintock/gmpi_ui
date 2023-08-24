/*************************************************************************
**                                                                       **
** EE.C         Expression Evaluator                                     **
**                                                                       **
** AUTHOR:      Mark Morley                                              **
** COPYRIGHT:   (c) 1992 by Mark Morley                                  **
** DATE:        December 1991                                            **
** HISTORY:     Jan 1992 - Made it squash all command line arguments     **
**                         into one big long string.                     **
**                       - It now can set/get VMS symbols as if they     **
**                         were variables.                               **
**                       - Changed max variable name length from 5 to 15 **
**              Jun 1992 - Updated comments and docs                     **
**                                                                       **
** You are free to incorporate this code into your own works, even if it **
** is a commercial application.  However, you may not charge anyone else **
** for the use of this code!  If you intend to distribute your code,     **
** I'd appreciate it if you left this message intact.  I'd like to       **
** receive credit wherever it is appropriate.  Thanks!                   **
**                                                                       **
** I don't promise that this code does what you think it does...         **
**                                                                       **
** Please mail any bug reports/fixes/enhancments to me at:               **
**      morley@camosun.bc.ca                                             **
** or                                                                    **
**      Mark Morley                                                      **
**      3889 Mildred Street                                              **
**      Victoria, BC  Canada                                             **
**      V8Z 7G1                                                          **
**      (604) 479-7861                                                   **
**                                                                       **
 *************************************************************************/

// !!! Turn off "Whole Program Optimisation".

#pragma warning (disable : 4996) // deprecated insecure calls (strcpy, memset etc)

#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include "expression_evaluate.h"
#include "xplatform.h"
#include "assert.h"

#define EVALUATOR_ERROR(n) {EE_ERROR=n; ERPOS=expression-ERANC-1; strcpy(ERTOK,token); throw EvaluatorException();}

//#define VAR             1
//#define DEL             2
//#define NUM             3
enum { VAR =1 , DEL, NUM };

typedef double(*math_function_pointer)(...);
typedef double(*math_function_pointer1)(double);
typedef double(*math_function_pointer2)(double, double);

typedef struct
{
	const char* name;                          /* Function name */
	int   args;                          /* Number of arguments to expect */
	double(*func)(...);                  /* Pointer to function */
} FUNCTION;


/* Codes returned from the evaluator */
enum {
E_OK         //  0        /* Successful evaluation */
,E_SYNTAX    //   1        /* Syntax error */
,E_UNBALAN   //   2        /* Unbalanced parenthesis */
,E_DIVZERO   //   3        /* Attempted division by zero */
,E_UNKNOWN   //   4        /* Reference to unknown variable */
,E_MAXVARS   //   5        /* Maximum variables exceeded */
,E_BADFUNC   //   6        /* Unrecognised function */
,E_NUMARGS   //   7        /* Wrong number of arguments to funtion */
,E_NOARG     //   8        /* Missing an argument to a funtion */
,E_EMPTY     //   9        /* Empty expression */
};

/*************************************************************************
**                                                                       **
** PROTOTYPES FOR CUSTOM MATH FUNCTIONS                                  **
**                                                                       **
 *************************************************************************/

double deg( double x );
double rad( double x );


/*************************************************************************
**                                                                       **
** VARIABLE DECLARATIONS                                                 **
**                                                                       **
 *************************************************************************/

int   EE_ERROR;              /* The error number */
char  ERTOK[Evaluator::TOKLEN + 1];     /* The token that generated the error */
int64_t   ERPOS;             /* The offset from the start of the expression */
char* ERANC;                 /* Used to calculate ERPOS */

/*
   Add any "constants" here...  These are "read-only" values that are
   provided as a convienence to the user.  Their values can not be
   permanently changed.  The first field is the variable name, the second
   is its value.
*/
Evaluator::VARIABLE Consts[] =
{
   /* name, value */
   { "pi",      M_PI },
   { "e",       M_E },

   { 0 }
};

// my own 'math functions'
double sgn( double x )
{
	if( x >= 0.0 )
		return 1.0;
	else
		return -1.0;
}

double my_max( double a, double b )
{
	return (std::max)(a,b);
}

double my_min( double a, double b )
{
	return (std::min)(a,b);
}

double my_hypot(double a, double b)
{
	return hypot(a, b);
}

/*
   Add any math functions that you wish to recognise here...  The first
   field is the name of the function as it would appear in an expression.
   The second field tells how many arguments to expect.  The third is
   a pointer to the actual function to use.
*/
FUNCTION Funcs[] =
{
   /* name, funtion to call */
   { "sin",     1,    (math_function_pointer)(math_function_pointer1) sin },
   { "cos",     1,    (math_function_pointer)(math_function_pointer1) cos },
   { "tan",     1,    (math_function_pointer)(math_function_pointer1) tan },
   { "asin",    1,    (math_function_pointer)(math_function_pointer1) asin },
   { "acos",    1,    (math_function_pointer)(math_function_pointer1) acos },
   { "atan",    1,    (math_function_pointer)(math_function_pointer1) atan },
   { "sinh",    1,    (math_function_pointer)(math_function_pointer1) sinh },
   { "cosh",    1,    (math_function_pointer)(math_function_pointer1) cosh },
   { "tanh",    1,    (math_function_pointer)(math_function_pointer1) tanh },
   { "exp",     1,    (math_function_pointer)(math_function_pointer1) exp },
   { "log",     1,    (math_function_pointer)(math_function_pointer1) log },
   { "log10",   1,    (math_function_pointer)(math_function_pointer1) log10 },
   { "pow",     2,    (math_function_pointer)(math_function_pointer2) pow },
   { "sqrt",    1,    (math_function_pointer)(math_function_pointer1) sqrt },
   { "floor",   1,    (math_function_pointer)(math_function_pointer1) floor },
   { "ceil",    1,    (math_function_pointer)(math_function_pointer1) ceil },
   { "abs",     1,    (math_function_pointer)(math_function_pointer1) fabs },
   { "hypot",   2,    (math_function_pointer)(math_function_pointer1) my_hypot },

   { "deg",     1,    (math_function_pointer)(math_function_pointer1) deg },
   { "rad",     1,    (math_function_pointer)(math_function_pointer1) rad },
   { "sgn",     1,    (math_function_pointer)(math_function_pointer1) sgn },
   { "min",     2,    (math_function_pointer)(math_function_pointer1) my_min },
   { "max",     2,    (math_function_pointer)(math_function_pointer1) my_max },

   { "" }
};


/*************************************************************************
**                                                                       **
** Some custom math functions...   Note that they must be prototyped     **
** above (if your compiler requires it)                                  **
**                                                                       **
** deg( x )             Converts x radians to degrees.                   **
** rad( x )             Converts x degrees to radians.                   **
**                                                                       **
 *************************************************************************/

double
deg( double x )
{
   return( x * 180.0 / M_PI );
}

double
rad( double x )
{
   return( x * M_PI / 180.0 );
}


/*************************************************************************
**                                                                       **
** ClearAllVars()                                                        **
**                                                                       **
** Erases all user-defined variables from memory. Note that constants    **
** can not be erased or modified in any way by the user.                 **
**                                                                       **
** Returns nothing.                                                      **
**                                                                       **
 *************************************************************************/

void Evaluator::ClearAllVars()
{
   int i;

   for( i = 0; i < MAXVARS; i++ )
   {
	  *Vars[i].name = 0;
	  Vars[i].value = 0;
   }
}


/*************************************************************************
**                                                                       **
** ClearVar( char* name )                                                **
**                                                                       **
** Erases the user-defined variable that is called NAME from memory.     **
** Note that constants are not affected.                                 **
**                                                                       **
** Returns 1 if the variable was found and erased, or 0 if it didn't     **
** exist.                                                                **
**                                                                       **
 *************************************************************************/

int Evaluator::ClearVar( const char* name )
{
   int i;

   for( i = 0; i < MAXVARS; i++ )
	  if( *Vars[i].name && ! strcmp( name, Vars[i].name ) )
	  {
		 *Vars[i].name = 0;
		 Vars[i].value = 0;
		 return( 1 );
	  }
   return( 0 );
}


/*************************************************************************
**                                                                       **
** GetValue( char* name, double* value )                                   **
**                                                                       **
** Looks up the specified variable (or constant) known as NAME and       **
** returns its contents in VALUE.                                        **
**                                                                       **
** First the user-defined variables are searched, then the constants are **
** searched.                                                             **
**                                                                       **
** Returns 1 if the value was found, or 0 if it wasn't.                  **
**                                                                       **
 *************************************************************************/

int Evaluator::GetValue( const char* name, double* value )
{
   int i;

   /* Now check the user-defined variables. */
   for( i = 0; i < MAXVARS; i++ )
	  if( *Vars[i].name && ! strcmp( name, Vars[i].name ) )
	  {
		 *value = Vars[i].value;
		 return( 1 );
	  }

   /* Now check the programmer-defined constants. */
   for( i = 0; *Consts[i].name; i++ )
	  if( *Consts[i].name && ! strcmp( name, Consts[i].name ) )
	  {
		 *value = Consts[i].value;
		 return( 1 );
	  }
   return( 0 );
}


/*************************************************************************
**                                                                       **
** SetValue( char* name, double* value )                                   **
**                                                                       **
** First, it erases any user-defined variable that is called NAME.  Then **
** it creates a new variable called NAME and gives it the value VALUE.   **
**                                                                       **
** Returns 1 if the value was added, or 0 if there was no more room.     **
**                                                                       **
 *************************************************************************/
int Evaluator::SetValue( const char* name, double* value )
{
   int  i;

   for( i = 0; i < MAXVARS; i++ )
   {
	  if( ! *Vars[i].name ) // empty slot, put var in here.
	  {
		 strcpy( Vars[i].name, name );
		//_strlwr( Vars[i].name ); // always lower case var names.
          std::transform(Vars[i].name,Vars[i].name+strlen(Vars[i].name),Vars[i].name,::tolower);
		 Vars[i].name[VARLEN] = 0;
		 Vars[i].value = *value;
		 return( 1 );
	  }

	  // used slot, name match?
	  if( ! strcmp( name, Vars[i].name ) )
	  {
		 Vars[i].value = *value;
		 return( 1 );
	  }
   }
   return( 0 );
}
/*************************************************************************
**                                                                       **
** Parse()   Internal use only                                           **
**                                                                       **
** This function is used to grab the next token from the expression that **
** is being evaluated.                                                   **
**                                                                       **
 *************************************************************************/

void Evaluator::Parse()
{
   char* t;

   type = 0;
   t = token;
   while( iswhite( *expression ) )
	  expression++;
   if( isdelim( *expression ) )
   {
	  type = DEL;
	  *t++ = *expression++;
   }
   else if( isnumer( *expression ) )
   {
	  type = NUM;
	  while( isnumer( *expression ) )
		 *t++ = *expression++;
   }
   else if( isalpha( *expression ) )
   {
	  type = VAR;
	  while( isalpha( *expression ) )
		*t++ = *expression++;
	  token[VARLEN] = 0;
   }
   else if( *expression )
   {
	  *t++ = *expression++;
	  *t = 0;
	  EVALUATOR_ERROR( E_SYNTAX );
   }
   *t = 0;
   while( iswhite( *expression ) )
	  expression++;
}

/*************************************************************************
**                                                                       **
** Level2( double* r )   Internal use only                                 **
**                                                                       **
** This function handles any addition and subtraction operations.        **
**                                                                       **
 *************************************************************************/


void Evaluator::Level2( double* r )
{
   double t = 0;
   char o;

   Level3( r );
   while( (o = *token) == '+' || o == '-' )
   {
	  Parse();
	  Level3( &t );
	  if( o == '+' )
		 *r = *r + t;
	  else if( o == '-' )
		 *r = *r - t;
   }
}


/*************************************************************************
**                                                                       **
** Level1( double* r )   Internal use only                                 **
**                                                                       **
** This function handles any variable assignment operations.             **
** It returns a value of 1 if it is a top-level assignment operation,    **
** otherwise it returns 0                                                **
**                                                                       **
 *************************************************************************/

int Evaluator::Level1( double* r )
{
   char t[VARLEN + 1];

   if( type == VAR )
	  if( *expression == '=' )
	  {
		 strcpy( t, token );
		 Parse();
		 Parse();
		 if( !*token )
		 {
			ClearVar( t );
			return(1);
		 }
		 Level2( r );
		 if( ! SetValue( t, r ) )
			EVALUATOR_ERROR( E_MAXVARS );
		 return( 1 );
	  }
   Level2( r );
   return( 0 );
}



/*************************************************************************
**                                                                       **
** Level3( double* r )   Internal use only                                 **
**                                                                       **
** This function handles any multiplication, division, or modulo.        **
**                                                                       **
 *************************************************************************/

void Evaluator::Level3( double* r )
{
   double t;
   char o;

   Level4( r );
   while( (o = *token) == '*' || o == '/' || o == '%' )
   {
	  Parse();
	  Level4( &t );
	  if( o == '*' )
		 *r = *r * t;
	  else if( o == '/' )
	  {
		 if( t == 0 )
			EVALUATOR_ERROR( E_DIVZERO );
		 *r = *r / t;
	  }
	  else if( o == '%' )
	  {
		 if( t == 0 )
			EVALUATOR_ERROR( E_DIVZERO );
		 *r = fmod( *r, t );
	  }
   }
}


/*************************************************************************
**                                                                       **
** Level4( double* r )   Internal use only                                 **
**                                                                       **
** This function handles any "to the power of" operations.               **
**                                                                       **
 *************************************************************************/

void Evaluator::Level4( double* r )
{
   double t;

   Level5( r );
   if( *token == '^' )
   {
	  Parse();
	  Level5( &t );
	  *r = pow( *r, t );
   }
}


/*************************************************************************
**                                                                       **
** Level5( double* r )   Internal use only                                 **
**                                                                       **
** This function handles any unary + or - signs.                         **
**                                                                       **
 *************************************************************************/

void Evaluator::Level5( double* r )
{
   char o = 0;

   if( *token == '+' || *token == '-' )
   {
	  o = *token;
	  Parse();
   }
   Level6( r );
   if( o == '-' )
	  *r = -*r;
}


/*************************************************************************
**                                                                       **
** Level6( double* r )   Internal use only                                 **
**                                                                       **
** This function handles any literal numbers, variables, or functions.   **
**                                                                       **
 *************************************************************************/

void Evaluator::Level6( double* r )
{
   int  i;
   int  n;
   double a[3];

   if( *token == '(' )
   {
	  Parse();
	  if( *token == ')' )
		 EVALUATOR_ERROR( E_NOARG );
	  Level1( r );
	  if( *token != ')' )
		 EVALUATOR_ERROR( E_UNBALAN );
	  Parse();
   }
   else
   {
	  if( type == NUM )
	  {
		 *r = (double) atof( token );
		 Parse();
	  }
	  else if( type == VAR )
	  {
		 if( *expression == '(' )
		 {
			for( i = 0; *Funcs[i].name; i++ )
			   if( ! strcmp( token, Funcs[i].name ) )
			   {
				  Parse();
				  n = 0;
				  do
				  {
					 Parse();
					 if( *token == ')' || *token == ',' )
						EVALUATOR_ERROR( E_NOARG );
					 a[n] = 0;
					 Level1( &a[n] );
					 n++;
				  } while( n < 4 && *token == ',' );
				  Parse();
				  if( n != Funcs[i].args )
				  {
					 strcpy( token, Funcs[i].name );
					 EVALUATOR_ERROR( E_NUMARGS );
				  }
				  *r = Funcs[i].func( a[0], a[1], a[2] );
				  return;
			   }
			   if( ! *Funcs[i].name )
				  EVALUATOR_ERROR( E_BADFUNC );
			}
			else if( ! GetValue( token, r ) )
			   EVALUATOR_ERROR( E_UNKNOWN );
		 Parse();
	  }
	  else
		 EVALUATOR_ERROR( E_SYNTAX );
   }
}


/*************************************************************************
**                                                                       **
** Evaluate( char* e, double* result, int* a )                             **
**                                                                       **
** This function is called to evaluate the expression E and return the   **
** answer in RESULT.  If the expression was a top-level assignment, a    **
** value of 1 will be returned in A, otherwise it will contain 0.        **
**                                                                       **
** Returns E_OK if the expression is valid, or an error code.            **
**                                                                       **
 *************************************************************************/

int Evaluator::Evaluate( const char* e, double* result, int* a )
{
//   if( setjmp( jb ) )
  //    return( EE_ERROR );

	try
	{
		char copy_of_expression[500];
		strcpy( copy_of_expression, e );

		expression = copy_of_expression;

		ERANC = copy_of_expression;
		// _strlwr( expression );
        std::transform(expression,expression+strlen(expression),expression,tolower);
		*result = 0;
		Parse();
		if( ! *token )
		  EVALUATOR_ERROR( E_EMPTY );
		*a = Level1( result );
	}
	catch( EvaluatorException )
	{
		*result = 0.0f;
		return( EE_ERROR );
	}

   return( E_OK );
}
