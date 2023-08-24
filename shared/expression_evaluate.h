/* Some of you may choose to define double as a "float" instead... */

class EvaluatorException
{
};

class Evaluator  
{
public:
	static const int MAXVARS = 50;              /* Max user-defined variables */
	static const int TOKLEN = 30;              /* Max token length */
	static const int VARLEN = 15;              /* Max length of variable names */

	typedef struct
	{
		char name[VARLEN + 1];               /* Variable name */
		double value;                          /* Variable value */
	} VARIABLE;

	Evaluator()
	{
		ClearAllVars(); // init expression evaluator
	}
	virtual ~Evaluator() {}

	int SetValue( const char* name, double* value );
	int Evaluate( const char* e, double* result, int* a );
	void ClearAllVars();
	int ClearVar( const char* name );
	int GetValue( const char* name, double* value );
	void Parse();

private:
	/* The following macros are ASCII dependant, no EBCDIC here! */
	inline bool iswhite(char c) {
		return  (c == ' ' || c == '\t');
	}

	inline bool isnumer(char c) {
		return   ((c >= '0' && c <= '9') || c == '.');
	}

	inline bool isalpha(char c) {
		return  ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') \
			|| c == '_');
	}

	inline bool isdelim(char c) {
		return  (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' \
			|| c == '^' || c == '(' || c == ')' || c == ',' || c == '=');
	}

	int Level1( double* r );
	void Level2( double* r );
	void Level3( double* r );
	void Level4( double* r );
	void Level5( double* r );
	void Level6( double* r );

	VARIABLE Vars[MAXVARS];       /* Array for user-defined variables */
	char*  expression;          /* Pointer to the user's expression */
	char   token[TOKLEN + 1];   /* Holds the current token */
	int             type;                /* Type of the current token */
};