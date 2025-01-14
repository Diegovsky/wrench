/*******************************************************************************
Copyright (c) 2022 Curt Hartung -- curt.hartung@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#include "wrench.h"
#ifndef WRENCH_WITHOUT_COMPILER
#include <assert.h>

#define WR_COMPILER_LITERAL_STRING 0x10 

//------------------------------------------------------------------------------
const char* c_reserved[] =
{
	"break",
	"case",
	"continue",
	"default",
	"do",
	"else",
	"false",
	"float",
	"for",
	"if",
	"int",
	"return",
	"switch",
	"true",
	"function",
	"while",
	"new",
	"struct",
	""
};

//#define _DUMP
#ifdef _DUMP
//------------------------------------------------------------------------------
const char* wr_asciiDump( const void* d, unsigned int len, WRstr& str )
{
	const unsigned char* data = (char unsigned *)d;
	str.clear();
	for( unsigned int i=0; i<len; i++ )
	{
		str.appendFormat( "0x%08X: ", i );
		char dump[24];
		unsigned int j;
		for( j=0; j<16 && i<len; j++, i++ )
		{
			dump[j] = isgraph((unsigned char)data[i]) ? data[i] : '.';
			dump[j+1] = 0;
			str.appendFormat( "%02X ", (unsigned char)data[i] );
		}

		for( ; j<16; j++ )
		{
			str.appendFormat( "   " );
		}
		i--;
		str += ": ";
		str += dump;
		str += "\n";
	}

	return str;
}
void streamDump( WROpcodeStream const& stream )
{
	WRstr str;
	wr_asciiDump( stream, stream.size(), str );
	printf( "\n%s\n", str.c_str() );
}

#endif

//------------------------------------------------------------------------------
bool WRCompilationContext::isValidLabel( WRstr& token, bool& isGlobal, WRstr& prefix )
{
	prefix.clear();

	if ( !token.size() || (!isalpha(token[0]) && token[0] != '_' && token[0] != ':') ) // non-zero size and start with alpha or '_' ?
	{
		return false;
	}

	isGlobal = false;

	if ( token[0] == ':' )
	{
		if ( (token.size() > 2) && (token[1] == ':') )
		{
			isGlobal = true;
		}
		else
		{
			return false;
		}
	}

	for( unsigned int i=0; c_reserved[i][0]; ++i )
	{
		if ( token == c_reserved[i] )
		{
			return false;
		}
	}

	for( unsigned int i=0; i<token.size(); i++ ) // entire token alphanumeric or '_'?
	{
		if ( token[i] == ':' )
		{
			if ( token[++i] != ':' )
			{
				m_err = WR_ERR_unexpected_token;
				return false;
			}

			if ( i != 1 )
			{
				for( unsigned int p=0; p<i - 1; ++p )
				{
					prefix += token[p];
				}

				for( unsigned int t=2; t<token.size(); ++t)
				{
					token[t-2] = token[t+prefix.size()];
				}
				token.shave( prefix.size() + 2 );
				i -= (prefix.size() + 1);
			}
			
			isGlobal = true;
			continue;
		}
		
		if ( !isalnum(token[i]) && token[i] != '_' )
		{
			return false;
		}
	}

	return true;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::getToken( WRExpressionContext& ex, const char* expect )
{
	WRValue& value = ex.value;
	WRstr& token = ex.token;
	
	if ( m_loadedToken.size() || (m_loadedValue.type != WR_REF) )
	{
		token = m_loadedToken;
		value = m_loadedValue;
	}
	else
	{
		value.p2 = INIT_AS_REF;

		ex.spaceBefore = (m_pos < m_sourceLen) && isspace(m_source[m_pos]);
	
		do
		{
			if ( m_pos >= m_sourceLen )
			{
				m_EOF = true;
				return false;
			}
			
		} while( isspace(m_source[m_pos++]) );

		token = m_source[m_pos - 1];

		if ( token[0] == '-' )
		{
			if ( m_pos < m_sourceLen )
			{
				if ( (isdigit(m_source[m_pos]) && !m_LastParsedLabel) || m_source[m_pos] == '.' )
				{
					goto parseAsNumber;
				}
				else if ( m_source[m_pos] == '-' )
				{
					token += '-';
					++m_pos;
				}
				else if ( m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
			}
		}
		else
		{
			m_LastParsedLabel = false;
		
			if ( token[0] == '=' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
			}
			else if ( token[0] == '!' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
			}
			else if ( token[0] == '*' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
			}
			else if ( token[0] == '%' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
			}
			else if ( token[0] == '<' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
				else if ( (m_pos < m_sourceLen) && m_source[m_pos] == '<' )
				{
					token += '<';
					++m_pos;

					if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
					{
						token += '=';
						++m_pos;
					}
				}
			}
			else if ( token[0] == '>' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
				else if ( (m_pos < m_sourceLen) && m_source[m_pos] == '>' )
				{
					token += '>';
					++m_pos;

					if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
					{
						token += '=';
						++m_pos;
					}
				}
			}
			else if ( token[0] == '&' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '&' )
				{
					token += '&';
					++m_pos;
				}
				else if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
			}
			else if ( token[0] == '^' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
			}
			else if ( token[0] == '|' )
			{
				if ( (m_pos < m_sourceLen) && m_source[m_pos] == '|' )
				{
					token += '|';
					++m_pos;
				}
				else if ( (m_pos < m_sourceLen) && m_source[m_pos] == '=' )
				{
					token += '=';
					++m_pos;
				}
			}
			else if ( token[0] == '+' )
			{
				if ( m_pos < m_sourceLen )
				{
					if ( m_source[m_pos] == '+' )
					{
						token += '+';
						++m_pos;
					}
					else if ( m_source[m_pos] == '=' )
					{
						token += '=';
						++m_pos;
					}
				}
			}
			else if ( token[0] == '\"' )
			{
				token.clear();

				do
				{
					if (m_pos >= m_sourceLen)
					{
						m_err = WR_ERR_unterminated_string_literal;
						m_EOF = true;
						return false;
					}

					char c = m_source[m_pos];
					if (c == '\"') // terminating character
					{
						++m_pos;
						break;
					}
					else if (c == '\n')
					{
						m_err = WR_ERR_newline_in_string_literal;
						return false;
					}
					else if (c == '\\')
					{
						c = m_source[++m_pos];
						if (m_pos >= m_sourceLen)
						{
							m_err = WR_ERR_unterminated_string_literal;
							m_EOF = true;
							return false;
						}

						if (c == '\\') // escaped slash
						{
							token += '\\';
						}
						else if (c == '0')
						{
							token += atoi(m_source + m_pos);
							for (; (m_pos < m_sourceLen) && isdigit(m_source[m_pos]); ++m_pos);
						}
						else if (c == '\"')
						{
							token += '\"';
						}
						else if (c == 'n')
						{
							token += '\n';
						}
						else if (c == 'r')
						{
							token += '\r';
						}
						else if (c == 't')
						{
							token += '\t';
						}
						else
						{
							m_err = WR_ERR_bad_string_escape_sequence;
							return false;
						}
					}
					else
					{
						token += c;
					}

				} while( ++m_pos < m_sourceLen );

				value.p2 = 0;
				value.type = (WRValueType)WR_COMPILER_LITERAL_STRING;
				value.p = &token;
			}
			else if ( token[0] == '/' ) // might be a comment
			{
				if ( m_pos < m_sourceLen )
				{	
					if ( !isspace(m_source[m_pos]) )
					{
						if ( m_source[m_pos] == '/' )
						{
							for( ; m_pos<m_sourceLen && m_source[m_pos] != '\n'; ++m_pos ); // clear to end EOL

							return getToken( ex, expect );
						}
						else if ( m_source[m_pos] == '*' )
						{
							for( ; (m_pos+1)<m_sourceLen
								 && !(m_source[m_pos] == '*' && m_source[m_pos+1] == '/');
								 ++m_pos ); // find end of comment

							m_pos += 2;

							return getToken( ex, expect );

						}
						else if ( m_source[m_pos] == '=' )
						{
							token += '=';
							++m_pos;
						}
					}					
					//else // bare '/' 
				}
			}
			else if ( token[0] == '.'
					  && m_source[m_pos] == '_'
					  && m_pos + 6 < m_sourceLen
					  && m_source[m_pos+1] == 'c'
					  && m_source[m_pos+2] == 'o'
					  && m_source[m_pos+3] == 'u'
					  && m_source[m_pos+4] == 'n'
					  && m_source[m_pos+5] == 't'
					  && isspace(m_source[m_pos+6]) )
			{
				m_pos += 6;
				token = "._count";
			}
			else if ( isdigit(token[0])
					  || (token[0] == '.' && isdigit(m_source[m_pos])) )
			{
				if ( m_pos >= m_sourceLen )
				{
					return false;
				}

parseAsNumber:

				m_LastParsedLabel = true;

				if ( token[0] == '0' && m_source[m_pos] == 'x' ) // interpret as hex
				{
					token.clear();
					m_pos++;

					for(;;)
					{
						if ( m_pos >= m_sourceLen )
						{
							m_err = WR_ERR_unexpected_EOF;
							return false;
						}

						if ( !isxdigit(m_source[m_pos]) )
						{
							break;
						}

						token += m_source[m_pos++];
					}

					value.p2 = INIT_AS_INT;
					value.i = strtol( token, 0, 16 );
				}
				else if (token[0] == '0' && m_source[m_pos] == 'b' )
				{
					token.clear();
					m_pos++;

					for(;;)
					{
						if ( m_pos >= m_sourceLen )
						{
							m_err = WR_ERR_unexpected_EOF;
							return false;
						}

						if ( !isxdigit(m_source[m_pos]) )
						{
							break;
						}

						token += m_source[m_pos++];
					}

					value.p2 = INIT_AS_INT;
					value.i = strtol( token, 0, 2 );
				}
				else
				{
					bool decimal = token[0] == '.';
					for(;;)
					{
						if ( m_pos >= m_sourceLen )
						{
							m_err = WR_ERR_unexpected_EOF;
							return false;
						}

						if ( m_source[m_pos] == '.' )
						{
							if ( decimal )
							{
								return false;
							}

							decimal = true;
						}
						else if ( !isdigit(m_source[m_pos]) )
						{
							break;
						}

						token += m_source[m_pos++];
					}

					if ( decimal )
					{
						value.p2 = INIT_AS_FLOAT;
						value.f = (float)atof( token );
					}
					else
					{
						value.p2 = INIT_AS_INT;
						value.i = strtol( token, 0, 10 );
					}
				}
			}
			else if ( isalpha(token[0]) || token[0] == '_' || token[0] == ':' ) // must be a label
			{
				m_LastParsedLabel = true;

				for( ; m_pos < m_sourceLen ; ++m_pos )
				{
					if ( !isalnum(m_source[m_pos]) && m_source[m_pos] != '_' && m_source[m_pos] != ':' )
					{
						break;
					}

					token += m_source[m_pos];
				}

				if ( token == "true" )
				{
					value.p2 = INIT_AS_INT;
					value.i = 1;
					token = "1";
				}
				else if ( token == "false" )
				{
					value.p2 = INIT_AS_INT;
					value.i = 0;
					token = "0";
				}
			}
		}

		ex.spaceAfter = (m_pos < m_sourceLen) && isspace(m_source[m_pos]);
	}

	m_loadedToken.clear();
	m_loadedValue.p2 = INIT_AS_REF;

	if ( expect && (token != expect) )
	{
		return false;
	}
	
	return true;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::CheckSkipLoad( WROpcode opcode, WRBytecode& bytecode, int a, int o )
{
	if ( bytecode.opcodes[o] == O_LoadFromLocal
		 && bytecode.opcodes[o-1] == O_LoadFromLocal )
	{
		bytecode.all[a-3] = O_LLValues;
		bytecode.all[a-1] = bytecode.all[a];
		bytecode.all[a] = opcode;
		bytecode.opcodes.shave(2);
		bytecode.opcodes += O_IndexSkipLoad;
		return true;
	}
	else if ( bytecode.opcodes[o] == O_LoadFromGlobal
			  && bytecode.opcodes[o-1] == O_LoadFromLocal )
	{
		bytecode.all[a-3] = O_LGValues;
		bytecode.all[a-1] = bytecode.all[a];
		bytecode.all[a] = opcode;
		bytecode.opcodes.shave(2);
		bytecode.opcodes += opcode;
		return true;
	}
	else if ( bytecode.opcodes[o] == O_LoadFromLocal
			  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
	{
		bytecode.all[a-3] = O_GLValues;
		bytecode.all[a-1] = bytecode.all[a];
		bytecode.all[a] = opcode;
		bytecode.opcodes.shave(2);
		bytecode.opcodes += opcode;
		return true;
	}
	else if ( bytecode.opcodes[o] == O_LoadFromGlobal
			  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
	{
		bytecode.all[a-3] = O_GGValues;
		bytecode.all[a-1] = bytecode.all[a];
		bytecode.all[a] = opcode;
		bytecode.opcodes.shave(2);
		bytecode.opcodes += opcode;
		return true;
	}

	return false;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::CheckFastLoad( WROpcode opcode, WRBytecode& bytecode, int a, int o )
{
	if ( a <= 2 || o == 0 )
	{
		return false;
	}

	if ( (opcode == O_Index && CheckSkipLoad(O_IndexSkipLoad, bytecode, a, o))
		 || (opcode == O_BinaryMod && CheckSkipLoad(O_BinaryModSkipLoad, bytecode, a, o)) 
		 || (opcode == O_BinaryRightShift && CheckSkipLoad(O_BinaryRightShiftSkipLoad, bytecode, a, o))
		 || (opcode == O_BinaryLeftShift && CheckSkipLoad(O_BinaryLeftShiftSkipLoad, bytecode, a, o)) 
		 || (opcode == O_BinaryAnd && CheckSkipLoad(O_BinaryAndSkipLoad, bytecode, a, o)) 
		 || (opcode == O_BinaryOr && CheckSkipLoad(O_BinaryOrSkipLoad, bytecode, a, o)) 
		 || (opcode == O_BinaryXOR && CheckSkipLoad(O_BinaryXORSkipLoad, bytecode, a, o)) )
	{
		return true;
	}
	
	return false;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::IsLiteralLoadOpcode( unsigned char opcode )
{
	return opcode == O_LiteralInt32
			|| opcode == O_LiteralZero
			|| opcode == O_LiteralFloat
			|| opcode == O_LiteralInt8
			|| opcode == O_LiteralInt16;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::CheckCompareReplace( WROpcode LS, WROpcode GS, WROpcode ILS, WROpcode IGS, WRBytecode& bytecode, unsigned int a, unsigned int o )
{
	if ( IsLiteralLoadOpcode(bytecode.opcodes[o-1]) )
	{
		if ( bytecode.opcodes[o] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 1 ] = GS;
			bytecode.opcodes[o] = GS;
			return true;
		}
		else if ( bytecode.opcodes[o] == O_LoadFromLocal )
		{
			bytecode.all[ a - 1 ] = LS;
			bytecode.opcodes[o] = LS;
			return true;
		}
	}
	else if ( IsLiteralLoadOpcode(bytecode.opcodes[o]) )
	{
		if ( (bytecode.opcodes[o] == O_LiteralInt32 || bytecode.opcodes[o] == O_LiteralFloat)
			 && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 6 ] = bytecode.all[ a ]; // store i3
			bytecode.all[ a ] = bytecode.all[ a - 5 ]; // move index

			bytecode.all[ a - 5 ] = bytecode.all[ a - 3 ];
			bytecode.all[ a - 4 ] = bytecode.all[ a - 2 ];
			bytecode.all[ a - 3 ] = bytecode.all[ a - 1 ];
			bytecode.all[ a - 2 ] = bytecode.all[ a - 6 ];
			bytecode.all[ a - 6 ] = bytecode.opcodes[o];
			bytecode.all[ a - 1 ] = IGS;

			bytecode.opcodes[o] = IGS; // reverse the logic

			return true;
		}
		else if ( bytecode.opcodes[o] == O_LiteralZero
				  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 2 ] = O_LiteralZero;
			bytecode.all[ a ] = bytecode.all[ a - 1];
			bytecode.all[ a - 1 ] = IGS;
			bytecode.opcodes[o] = IGS; // reverse the logic
			return true;
		}
		else if ( bytecode.opcodes[o] == O_LiteralZero
				  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 2 ] = O_LiteralZero;
			bytecode.all[ a ] = bytecode.all[ a - 1];
			bytecode.all[ a - 1 ] = IGS;
			bytecode.opcodes[o] = IGS; // reverse the logic
			return true;
		}
		else if ( bytecode.opcodes[o] == O_LiteralInt8
				  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 1 ] = bytecode.all[ a - 2 ];
			bytecode.all[ a - 2 ] = bytecode.all[ a ];
			bytecode.all[ a ] = bytecode.all[ a - 1 ];
			bytecode.all[ a - 1 ] = IGS;
			bytecode.all[ a - 3 ] = O_LiteralInt8;

			bytecode.opcodes[o] = IGS; // reverse the logic
			return true;
		}
		else if ( bytecode.opcodes[o] == O_LiteralInt16
				  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 2 ] = bytecode.all[ a ];

			bytecode.all[ a - 4 ] = bytecode.all[ a - 3 ];
			bytecode.all[ a - 3 ] = bytecode.all[ a - 1 ];
			bytecode.all[ a ] = bytecode.all[ a - 4 ];
			bytecode.all[ a - 1 ] = IGS;
			bytecode.all[ a - 4 ] = O_LiteralInt16;

			bytecode.opcodes[o] = IGS; // reverse the logic
			return true;
		}
		if ( (bytecode.opcodes[o] == O_LiteralInt32 || bytecode.opcodes[o] == O_LiteralFloat)
			 && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 6 ] = bytecode.all[ a ]; // store i3
			bytecode.all[ a ] = bytecode.all[ a - 5 ]; // move index

			bytecode.all[ a - 5 ] = bytecode.all[ a - 3 ];
			bytecode.all[ a - 4 ] = bytecode.all[ a - 2 ];
			bytecode.all[ a - 3 ] = bytecode.all[ a - 1 ];
			bytecode.all[ a - 2 ] = bytecode.all[ a - 6 ];
			bytecode.all[ a - 6 ] = bytecode.opcodes[o];
			bytecode.all[ a - 1 ] = ILS;

			bytecode.opcodes[o] = ILS; // reverse the logic

			return true;
		}
		else if ( bytecode.opcodes[o] == O_LiteralZero
				  && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 2 ] = O_LiteralZero;
			bytecode.all[ a ] = bytecode.all[ a - 1];
			bytecode.all[ a - 1 ] = ILS;
			bytecode.opcodes[o] = ILS; // reverse the logic
			return true;
		}
		else if ( bytecode.opcodes[o] == O_LiteralZero
				  && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 2 ] = O_LiteralZero;
			bytecode.all[ a ] = bytecode.all[ a - 1];
			bytecode.all[ a - 1 ] = ILS;
			bytecode.opcodes[o] = ILS; // reverse the logic
			return true;
		}
		else if ( bytecode.opcodes[o] == O_LiteralInt8
				  && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 1 ] = bytecode.all[ a - 2 ];
			bytecode.all[ a - 2 ] = bytecode.all[ a ];
			bytecode.all[ a ] = bytecode.all[ a - 1 ];
			bytecode.all[ a - 1 ] = ILS;
			bytecode.all[ a - 3 ] = O_LiteralInt8;

			bytecode.opcodes[o] = ILS; // reverse the logic
			return true;
		}
		else if ( bytecode.opcodes[o] == O_LiteralInt16
				  && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 2 ] = bytecode.all[ a ];

			bytecode.all[ a - 4 ] = bytecode.all[ a - 3 ];
			bytecode.all[ a - 3 ] = bytecode.all[ a - 1 ];
			bytecode.all[ a ] = bytecode.all[ a - 4 ];
			bytecode.all[ a - 1 ] = ILS;
			bytecode.all[ a - 4 ] = O_LiteralInt16;

			bytecode.opcodes[o] = ILS; // reverse the logic
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
void WRCompilationContext::pushOpcode( WRBytecode& bytecode, WROpcode opcode )
{
	unsigned int o = bytecode.opcodes.size();
	if ( o )
	{
		// keyhole optimizations

		--o;
		unsigned int a = bytecode.all.size() - 1;

		if ( opcode == O_CompareEQ && o>0 && bytecode.opcodes[o] == O_LoadFromLocal && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 3 ] = O_LLCompareEQ;
			bytecode.all[ a - 1 ] = bytecode.all[ a ];
			bytecode.all.shave( 1 );
			bytecode.opcodes.shave( 2 );
			bytecode.opcodes += O_LLCompareEQ;
			return;
		}
		else if ( opcode == O_CompareNE && o>0 && bytecode.opcodes[o] == O_LoadFromLocal && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 3 ] = O_LLCompareNE;
			bytecode.all[ a - 1 ] = bytecode.all[ a ];
			bytecode.all.shave( 1 );
			bytecode.opcodes.shave( 2 );
			bytecode.opcodes += O_LLCompareNE;
			return;
		}
		else if ( opcode == O_CompareGT && o>0 && bytecode.opcodes[o] == O_LoadFromLocal && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 3 ] = O_LLCompareGT;
			bytecode.all[ a - 1 ] = bytecode.all[ a ];
			bytecode.all.shave( 1 );
			bytecode.opcodes.shave( 2 );
			bytecode.opcodes += O_LLCompareGT;
			return;
		}
		else if ( opcode == O_CompareLT && o>0 && bytecode.opcodes[o] == O_LoadFromLocal && bytecode.opcodes[o-1] == O_LoadFromLocal )
		{
			bytecode.all[ a - 3 ] = O_LLCompareLT;
			bytecode.all[ a - 1 ] = bytecode.all[ a ];
			bytecode.all.shave( 1 );
			bytecode.opcodes.shave( 2 );
			bytecode.opcodes += O_LLCompareLT;
			return;
		}
		else if ( opcode == O_CompareEQ && o>0 && bytecode.opcodes[o] == O_LoadFromGlobal && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 3 ] = O_GGCompareEQ;
			bytecode.all[ a - 1 ] = bytecode.all[ a ];
			bytecode.all.shave( 1 );
			bytecode.opcodes.shave( 2 );
			bytecode.opcodes += O_GGCompareEQ;
			return;
		}
		else if ( opcode == O_CompareNE && o>0 && bytecode.opcodes[o] == O_LoadFromGlobal && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 3 ] = O_GGCompareNE;
			bytecode.all[ a - 1 ] = bytecode.all[ a ];
			bytecode.all.shave( 1 );
			bytecode.opcodes.shave( 2 );
			bytecode.opcodes += O_GGCompareNE;
			return;
		}
		else if ( opcode == O_CompareGT && o>0 && bytecode.opcodes[o] == O_LoadFromGlobal && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 3 ] = O_GGCompareGT;
			bytecode.all[ a - 1 ] = bytecode.all[ a ];
			bytecode.all.shave( 1 );
			bytecode.opcodes.shave( 2 );
			bytecode.opcodes += O_GGCompareGT;
			return;
		}
		else if ( opcode == O_CompareLT && o>0 && bytecode.opcodes[o] == O_LoadFromGlobal && bytecode.opcodes[o-1] == O_LoadFromGlobal )
		{
			bytecode.all[ a - 3 ] = O_GGCompareLT;
			bytecode.all[ a - 1 ] = bytecode.all[ a ];
			bytecode.all.shave( 1 );
			bytecode.opcodes.shave( 2 );
			bytecode.opcodes += O_GGCompareLT;
			return;
		}
		else if ( (opcode == O_CompareEQ && (o>0))
				  && CheckCompareReplace(O_LSCompareEQ, O_GSCompareEQ, O_LSCompareEQ, O_GSCompareEQ, bytecode, a, o) )
		{
			return;
		}
		else if ( (opcode == O_CompareNE && (o>0))
				  && CheckCompareReplace(O_LSCompareNE, O_GSCompareNE, O_LSCompareNE, O_GSCompareNE, bytecode, a, o) )
		{
			return;
		}
		else if ( (opcode == O_CompareGE && (o>0))
			 && CheckCompareReplace(O_LSCompareGE, O_GSCompareGE, O_LSCompareLT, O_GSCompareLT, bytecode, a, o) )
		{
			return;
		}
		else if ( (opcode == O_CompareLE && (o>0))
				  && CheckCompareReplace(O_LSCompareLE, O_GSCompareLE, O_LSCompareGT, O_GSCompareGT, bytecode, a, o) )
		{
			return;
		}
		else if ( (opcode == O_CompareGT && (o>0))
				  && CheckCompareReplace(O_LSCompareGT, O_GSCompareGT, O_LSCompareLE, O_GSCompareLE, bytecode, a, o) )
		{
			return;
		}
		else if ( (opcode == O_CompareLT && (o>0))
				  && CheckCompareReplace(O_LSCompareLT, O_GSCompareLT, O_LSCompareGE, O_GSCompareGE, bytecode, a, o) )
		{
			return;
		}
		else if ( CheckFastLoad(opcode, bytecode, a, o) )
		{
			return;
		}
		else if ( opcode == O_BinaryMultiplication && (a>2) )
		{
			
			if ( bytecode.opcodes[o] == O_LoadFromGlobal
				 && bytecode.opcodes[o-1] == O_LoadFromGlobal )
			{
				// LoadFromGlobal   a - 3
				// [index]          a - 2
				// LoadFromGlobal   a - 1
				// [index]          a

				bytecode.all[ a - 3 ] = O_GGBinaryMultiplication;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];

				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromLocal
					  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
			{
				bytecode.all[ a - 3 ] = O_GLBinaryMultiplication;
				bytecode.all[ a - 1 ] = bytecode.all[ a - 2 ];
				bytecode.all[ a - 2 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromGlobal
					  && bytecode.opcodes[o-1] == O_LoadFromLocal )
			{
				bytecode.all[ a - 3 ] = O_GLBinaryMultiplication;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromLocal
					  && bytecode.opcodes[o-1] == O_LoadFromLocal )
			{
				bytecode.all[ a - 3 ] = O_LLBinaryMultiplication;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else
			{
				bytecode.all += opcode;
				bytecode.opcodes += opcode;
			}
			
			return;
		}
		else if ( opcode == O_BinaryAddition && (a>2) )
		{
			if ( bytecode.opcodes[o] == O_LoadFromGlobal
				 && bytecode.opcodes[o-1] == O_LoadFromGlobal )
			{
				bytecode.all[ a - 3 ] = O_GGBinaryAddition;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];

				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromLocal
					  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
			{
				bytecode.all[ a - 3 ] = O_GLBinaryAddition;
				bytecode.all[ a - 1 ] = bytecode.all[ a - 2 ];
				bytecode.all[ a - 2 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromGlobal
					  && bytecode.opcodes[o-1] == O_LoadFromLocal )
			{
				bytecode.all[ a - 3 ] = O_GLBinaryAddition;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromLocal
					  && bytecode.opcodes[o-1] == O_LoadFromLocal )
			{
				bytecode.all[ a - 3 ] = O_LLBinaryAddition;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else
			{
				bytecode.all += opcode;
				bytecode.opcodes += opcode;
			}

			return;
		}
		else if ( opcode == O_BinarySubtraction && (a>2) )
		{
			if ( bytecode.opcodes[o] == O_LoadFromGlobal
				 && bytecode.opcodes[o-1] == O_LoadFromGlobal )
			{
				bytecode.all[ a - 3 ] = O_GGBinarySubtraction;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];

				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromLocal
					  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
			{
				bytecode.all[ a - 3 ] = O_LGBinarySubtraction;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromGlobal
					  && bytecode.opcodes[o-1] == O_LoadFromLocal )
			{
				bytecode.all[ a - 3 ] = O_GLBinarySubtraction;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromLocal
					  && bytecode.opcodes[o-1] == O_LoadFromLocal )
			{
				bytecode.all[ a - 3 ] = O_LLBinarySubtraction;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else
			{
				bytecode.all += opcode;
				bytecode.opcodes += opcode;
			}

			return;
		}
		else if ( opcode == O_BinaryDivision && (a>2) )
		{
			if ( bytecode.opcodes[o] == O_LoadFromGlobal
				 && bytecode.opcodes[o-1] == O_LoadFromGlobal )
			{
				bytecode.all[ a - 3 ] = O_GGBinaryDivision;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromLocal
					  && bytecode.opcodes[o-1] == O_LoadFromGlobal )
			{
				bytecode.all[ a - 3 ] = O_LGBinaryDivision;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromGlobal
					  && bytecode.opcodes[o-1] == O_LoadFromLocal )
			{
				bytecode.all[ a - 3 ] = O_GLBinaryDivision;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else if ( bytecode.opcodes[o] == O_LoadFromLocal
					  && bytecode.opcodes[o-1] == O_LoadFromLocal )
			{
				bytecode.all[ a - 3 ] = O_LLBinaryDivision;
				bytecode.all[ a - 1 ] = bytecode.all[ a ];
				bytecode.all.shave(1);
				bytecode.opcodes.shave(2);
			}
			else
			{
				bytecode.all += opcode;
				bytecode.opcodes += opcode;
			}

			return;
		}
		else if ( opcode == O_Index )
		{
			if ( bytecode.opcodes[o] == O_LiteralInt16 )
			{
				bytecode.all[a-2] = O_IndexLiteral16;
				bytecode.opcodes[o] = O_IndexLiteral16;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LiteralInt8 )
			{
				bytecode.all[a-1] = O_IndexLiteral8;
				bytecode.opcodes[o] = O_IndexLiteral8;
				return;
			}
		}
		else if ( opcode == O_BZ )
		{
			if ( bytecode.opcodes[o] == O_LLCompareLT )
			{
				bytecode.opcodes[o] = O_LLCompareLTBZ;
				bytecode.all[ a - 2 ] = O_LLCompareLTBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LLCompareGT )
			{
				bytecode.opcodes[o] = O_LLCompareGTBZ;
				bytecode.all[ a - 2 ] = O_LLCompareGTBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LLCompareEQ )
			{
				bytecode.opcodes[o] = O_LLCompareEQBZ;
				bytecode.all[ a - 2 ] = O_LLCompareEQBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LLCompareNE )
			{
				bytecode.opcodes[o] = O_LLCompareNEBZ;
				bytecode.all[ a - 2 ] = O_LLCompareNEBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GGCompareLT )
			{
				bytecode.opcodes[o] = O_GGCompareLTBZ;
				bytecode.all[ a - 2 ] = O_GGCompareLTBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GGCompareGT )
			{
				bytecode.opcodes[o] = O_GGCompareGTBZ;
				bytecode.all[ a - 2 ] = O_GGCompareGTBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GGCompareEQ )
			{
				bytecode.opcodes[o] = O_GGCompareEQBZ;
				bytecode.all[ a - 2 ] = O_GGCompareEQBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GGCompareNE )
			{
				bytecode.opcodes[o] = O_GGCompareNEBZ;
				bytecode.all[ a - 2 ] = O_GGCompareNEBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GSCompareEQ )
			{
				bytecode.opcodes[o] = O_GSCompareEQBZ;
				bytecode.all[ a - 1 ] = O_GSCompareEQBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LSCompareEQ )
			{
				bytecode.opcodes[o] = O_LSCompareEQBZ;
				bytecode.all[ a - 1 ] = O_LSCompareEQBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GSCompareNE )
			{
				bytecode.opcodes[o] = O_GSCompareNEBZ;
				bytecode.all[ a - 1 ] = O_GSCompareNEBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LSCompareNE )
			{
				bytecode.opcodes[o] = O_LSCompareNEBZ;
				bytecode.all[ a - 1 ] = O_LSCompareNEBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GSCompareGE )
			{
				bytecode.opcodes[o] = O_GSCompareGEBZ;
				bytecode.all[ a - 1 ] = O_GSCompareGEBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LSCompareGE )
			{
				bytecode.opcodes[o] = O_LSCompareGEBZ;
				bytecode.all[ a - 1 ] = O_LSCompareGEBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GSCompareLE )
			{
				bytecode.opcodes[o] = O_GSCompareLEBZ;
				bytecode.all[ a - 1 ] = O_GSCompareLEBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LSCompareLE )
			{
				bytecode.opcodes[o] = O_LSCompareLEBZ;
				bytecode.all[ a - 1 ] = O_LSCompareLEBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GSCompareGT )
			{
				bytecode.opcodes[o] = O_GSCompareGTBZ;
				bytecode.all[ a - 1 ] = O_GSCompareGTBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LSCompareGT )
			{
				bytecode.opcodes[o] = O_LSCompareGTBZ;
				bytecode.all[ a - 1 ] = O_LSCompareGTBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_GSCompareLT )
			{
				bytecode.opcodes[o] = O_GSCompareLTBZ;
				bytecode.all[ a - 1 ] = O_GSCompareLTBZ;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LSCompareLT )
			{
				bytecode.opcodes[o] = O_LSCompareLTBZ;
				bytecode.all[ a - 1 ] = O_LSCompareLTBZ;
				return;
			}			
			else if ( bytecode.opcodes[o] == O_CompareEQ ) // assign+pop is very common
			{
				if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromLocal) && (bytecode.opcodes[o-2] == O_LoadFromLocal) )
				{
					bytecode.all[a-4] = O_LLCompareEQBZ;
					bytecode.all[a-2] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_LLCompareEQBZ;
					bytecode.all.shave(2);
				}
				else if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromGlobal) && (bytecode.opcodes[o-2] == O_LoadFromGlobal) )
				{
					bytecode.all[a-4] = O_GGCompareEQBZ;
					bytecode.all[a-2] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_GGCompareEQBZ;
					bytecode.all.shave(2);
				}
				else
				{
					bytecode.all[a] = O_CompareBEQ;
					bytecode.opcodes[o] = O_CompareBEQ;
				}
				return;
			}
			else if ( bytecode.opcodes[o] == O_CompareLT ) // assign+pop is very common
			{
				if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromLocal) && (bytecode.opcodes[o-2] == O_LoadFromLocal) )
				{
					bytecode.all[a-4] = O_LLCompareLTBZ;
					bytecode.all[a-2] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_LLCompareLTBZ;
					bytecode.all.shave(2);
				}
				else if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromGlobal) && (bytecode.opcodes[o-2] == O_LoadFromGlobal) )
				{
					bytecode.all[a-4] = O_GGCompareLTBZ;
					bytecode.all[a-2] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_GGCompareLTBZ;
					bytecode.all.shave(2);
				}
				else
				{
					bytecode.all[a] = O_CompareBLT;
					bytecode.opcodes[o] = O_CompareBLT;
				}
				return;
			}
			else if ( bytecode.opcodes[o] == O_CompareGT ) // assign+pop is very common
			{
				if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromLocal) && (bytecode.opcodes[o-2] == O_LoadFromLocal) )
				{
					bytecode.all[a-4] = O_LLCompareGTBZ;
					bytecode.all[a-2] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_LLCompareGTBZ;
					bytecode.all.shave(2);
				}
				else if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromGlobal) && (bytecode.opcodes[o-2] == O_LoadFromGlobal) )
				{
					bytecode.all[a-4] = O_GGCompareGTBZ;
					bytecode.all[a-2] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_GGCompareGTBZ;
					bytecode.all.shave(2);
				}
				else
				{
					bytecode.all[a] = O_CompareBGT;
					bytecode.opcodes[o] = O_CompareBGT;
				}
				return;
			}
			else if ( bytecode.opcodes[o] == O_CompareGE ) // assign+pop is very common
			{
				if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromLocal) && (bytecode.opcodes[o-2] == O_LoadFromLocal) )
				{
					bytecode.all[a-4] = O_LLCompareLTBZ;
					bytecode.all[a-2] = bytecode.all[a-3];
					bytecode.all[a-3] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_LLCompareLTBZ;
					bytecode.all.shave(2);
				}
				else if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromGlobal) && (bytecode.opcodes[o-2] == O_LoadFromGlobal) )
				{
					bytecode.all[a-4] = O_GGCompareLTBZ;
					bytecode.all[a-2] = bytecode.all[a-3];
					bytecode.all[a-3] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_GGCompareLTBZ;
					bytecode.all.shave(2);
				}
				else
				{
					bytecode.all[a] = O_CompareBGE;
					bytecode.opcodes[o] = O_CompareBGE;
				}
				return;
			}
			else if ( bytecode.opcodes[o] == O_CompareLE ) // assign+pop is very common
			{
				if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromLocal) && (bytecode.opcodes[o-2] == O_LoadFromLocal) )
				{
					bytecode.all[a-4] = O_LLCompareGTBZ;
					bytecode.all[a-2] = bytecode.all[a-3];
					bytecode.all[a-3] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_LLCompareGTBZ;
					bytecode.all.shave(2);
				}
				else if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromGlobal) && (bytecode.opcodes[o-2] == O_LoadFromGlobal) )
				{
					bytecode.all[a-4] = O_GGCompareGTBZ;
					bytecode.all[a-2] = bytecode.all[a-3];
					bytecode.all[a-3] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_GGCompareGTBZ;
					bytecode.all.shave(2);
				}
				else
				{
					bytecode.all[a] = O_CompareBLE;
					bytecode.opcodes[o] = O_CompareBLE;
				}

				return;
			}
			else if ( bytecode.opcodes[o] == O_CompareNE ) // assign+pop is very common
			{
				if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromLocal) && (bytecode.opcodes[o-2] == O_LoadFromLocal) )
				{
					bytecode.all[a-4] = O_LLCompareNEBZ;
					bytecode.all[a-2] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_LLCompareNEBZ;
					bytecode.all.shave(2);
				}
				else if ( (o>1) && (bytecode.opcodes[o-1] == O_LoadFromGlobal) && (bytecode.opcodes[o-2] == O_LoadFromGlobal) )
				{
					bytecode.all[a-4] = O_GGCompareNEBZ;
					bytecode.all[a-2] = bytecode.all[a-1];
					bytecode.opcodes.shave(2);
					bytecode.opcodes[o-2] = O_GGCompareNEBZ;
					bytecode.all.shave(2);
				}
				else
				{
					bytecode.all[a] = O_CompareBNE;
					bytecode.opcodes[o] = O_CompareBNE;
				}
				return;
			}
			else if ( bytecode.opcodes[o] == O_LogicalOr ) // assign+pop is very common
			{
				bytecode.all[a] = O_BLO;
				bytecode.opcodes[o] = O_BLO;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LogicalAnd ) // assign+pop is very common
			{
				bytecode.all[a] = O_BLA;
				bytecode.opcodes[o] = O_BLA;
				return;
			}
		}
		else if ( opcode == O_PopOne )
		{
			if ( bytecode.opcodes[o] == O_LoadFromLocal || bytecode.opcodes[o] == O_LoadFromGlobal ) 
			{
				bytecode.all.shave(2);
				bytecode.opcodes.shave(1);
				return;
			}
			else if ( bytecode.opcodes[o] == O_FUNCTION_CALL_PLACEHOLDER )
			{
				bytecode.all[ a ] = O_PopOne;
				return;
			}
			else if ( bytecode.opcodes[o] == O_PreIncrement || bytecode.opcodes[o] == O_PostIncrement )
			{	
				if ( (o > 0) && bytecode.opcodes[o-1] == O_LoadFromGlobal )
				{
					bytecode.all[ a - 2 ] = O_IncGlobal;
										
					bytecode.all.shave(1);
					bytecode.opcodes.shave(1);
				}
				else if ( (o > 0) && bytecode.opcodes[o-1] == O_LoadFromLocal )
				{
					bytecode.all[ a - 2 ] = O_IncLocal;
					bytecode.all.shave(1);
					bytecode.opcodes.shave(1);
				}
				else
				{
					bytecode.all[a] = O_PreIncrementAndPop;
					bytecode.opcodes[o] = O_PreIncrementAndPop;
				}

				return;
			}
			else if ( bytecode.opcodes[o] == O_PreDecrement || bytecode.opcodes[o] == O_PostDecrement )
			{	
				if ( (o > 0) && bytecode.opcodes[o-1] == O_LoadFromGlobal )
				{
					bytecode.all[ a - 2 ] = O_DecGlobal;
					bytecode.all.shave(1);
					bytecode.opcodes.shave(1);
				}
				else if ( (o > 0) && bytecode.opcodes[o-1] == O_LoadFromLocal )
				{
					bytecode.all[ a - 2 ] = O_DecLocal;
					bytecode.all.shave(1);
					bytecode.opcodes.shave(1);
				}
				else
				{
					bytecode.all[a] = O_PreDecrementAndPop;
					bytecode.opcodes[o] = O_PreDecrementAndPop;
				}
				
				return;
			}
			else if ( bytecode.opcodes[o] == O_CallLibFunction )
			{
				bytecode.all[a-5] = O_CallLibFunctionAndPop;
				bytecode.opcodes[o] = O_CallLibFunctionAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_Assign ) // assign+pop is very common
			{
				if ( o > 0 )
				{
					if ( bytecode.opcodes[o-1] == O_LoadFromGlobal )
					{
						if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralInt8 )
						{
							bytecode.all[ a - 4 ] = O_LiteralInt8ToGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 3 ];
							bytecode.all[ a - 3 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralInt16 )
						{
							bytecode.all[ a - 5 ] = O_LiteralInt16ToGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 3 ];
							bytecode.all[ a - 3 ] = bytecode.all[ a - 4 ];
							bytecode.all[ a - 4 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralInt32 )
						{
							bytecode.all[ a - 7 ] = O_LiteralInt32ToGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 3 ];
							bytecode.all[ a - 3 ] = bytecode.all[ a - 4 ];
							bytecode.all[ a - 4 ] = bytecode.all[ a - 5 ];
							bytecode.all[ a - 5 ] = bytecode.all[ a - 6 ];
							bytecode.all[ a - 6 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralFloat )
						{
							bytecode.all[ a - 7 ] = O_LiteralFloatToGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 3 ];
							bytecode.all[ a - 3 ] = bytecode.all[ a - 4 ];
							bytecode.all[ a - 4 ] = bytecode.all[ a - 5 ];
							bytecode.all[ a - 5 ] = bytecode.all[ a - 6 ];
							bytecode.all[ a - 6 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( (o > 1) && bytecode.opcodes[o-2] == O_LiteralZero )
						{
							bytecode.all[ a - 3 ] = O_LiteralInt8ToGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all[ a - 1 ] = 0;
							bytecode.all.shave(1);
							bytecode.opcodes.shave(2);
						}
						else if ( (o > 1) && bytecode.opcodes[o - 2] == O_BinaryDivision )
						{
							bytecode.all[ a - 3 ] = O_BinaryDivisionAndStoreGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( (o > 1) && bytecode.opcodes[o - 2] == O_BinaryAddition )
						{
							bytecode.all[ a - 3 ] = O_BinaryAdditionAndStoreGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( (o > 1) && bytecode.opcodes[o - 2] == O_BinaryMultiplication )
						{
							bytecode.all[ a - 3 ] = O_BinaryMultiplicationAndStoreGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( (o > 1) && bytecode.opcodes[o - 2] == O_BinarySubtraction )
						{
							bytecode.all[ a - 3 ] = O_BinarySubtractionAndStoreGlobal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ((o > 1) && bytecode.opcodes[o - 2] == O_FUNCTION_CALL_PLACEHOLDER )
						{
							//bytecode.all[a] = bytecode.all[a-1];
							bytecode.all[a-2] = O_AssignToGlobalAndPop;
							bytecode.all.shave(1);
							bytecode.opcodes[o] = O_AssignToGlobalAndPop;
						}
						else
						{
							bytecode.all[ a - 2 ] = O_AssignToGlobalAndPop;
							bytecode.all.shave(1);
							bytecode.opcodes.shave(1);
						}

						return;
					}
					else if ( bytecode.opcodes[ o - 1 ] == O_LoadFromLocal )
					{
						if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralInt8 )
						{
							bytecode.all[ a - 4 ] = O_LiteralInt8ToLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 3 ];
							bytecode.all[ a - 3 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralInt16 )
						{
							bytecode.all[ a - 5 ] = O_LiteralInt16ToLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 3 ];
							bytecode.all[ a - 3 ] = bytecode.all[ a - 4 ];
							bytecode.all[ a - 4 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralInt32 )
						{
							bytecode.all[ a - 7 ] = O_LiteralInt32ToLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 3 ];
							bytecode.all[ a - 3 ] = bytecode.all[ a - 4 ];
							bytecode.all[ a - 4 ] = bytecode.all[ a - 5 ];
							bytecode.all[ a - 5 ] = bytecode.all[ a - 6 ];
							bytecode.all[ a - 6 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralFloat )
						{
							bytecode.all[ a - 7 ] = O_LiteralFloatToLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 3 ];
							bytecode.all[ a - 3 ] = bytecode.all[ a - 4 ];
							bytecode.all[ a - 4 ] = bytecode.all[ a - 5 ];
							bytecode.all[ a - 5 ] = bytecode.all[ a - 6 ];
							bytecode.all[ a - 6 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( o > 1 && bytecode.opcodes[o-2] == O_LiteralZero )
						{
							bytecode.all[ a - 3 ] = O_LiteralInt8ToLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all[ a - 1 ] = 0;
							bytecode.all.shave(1);
							bytecode.opcodes.shave(1);
						}
						else if ( bytecode.opcodes[o - 2] == O_BinaryDivision )
						{
							bytecode.all[a - 3] = O_BinaryDivisionAndStoreLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( bytecode.opcodes[o - 2] == O_BinaryAddition )
						{
							bytecode.all[a - 3] = O_BinaryAdditionAndStoreLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( bytecode.opcodes[o - 2] == O_BinaryMultiplication )
						{
							bytecode.all[a - 3] = O_BinaryMultiplicationAndStoreLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else if ( bytecode.opcodes[o - 2] == O_BinarySubtraction )
						{
							bytecode.all[a - 3] = O_BinarySubtractionAndStoreLocal;
							bytecode.all[ a - 2 ] = bytecode.all[ a - 1 ];
							bytecode.all.shave(2);
							bytecode.opcodes.shave(2);
						}
						else
						{
							bytecode.all[ a - 2 ] = O_AssignToLocalAndPop;
							bytecode.all.shave(1);
							bytecode.opcodes.shave(1);
						}
						return;
					}
				}
								
				bytecode.all[a] = O_AssignAndPop;
				bytecode.opcodes[o] = O_AssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LiteralZero ) // put a zero on just to pop it off..
			{
				bytecode.opcodes.shave(1);
				bytecode.all.shave(1);
				return;
			}
			else if ( bytecode.opcodes[o] == O_SubtractAssign )
			{
				bytecode.all[a] = O_SubtractAssignAndPop;
				bytecode.opcodes[o] = O_SubtractAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_AddAssign )
			{
				bytecode.all[a] = O_AddAssignAndPop;
				bytecode.opcodes[o] = O_AddAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_ModAssign )
			{
				bytecode.all[a] = O_ModAssignAndPop;
				bytecode.opcodes[o] = O_ModAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_MultiplyAssign )
			{
				bytecode.all[a] = O_MultiplyAssignAndPop;
				bytecode.opcodes[o] = O_MultiplyAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_DivideAssign )
			{
				bytecode.all[a] = O_DivideAssignAndPop;
				bytecode.opcodes[o] = O_DivideAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_ORAssign )
			{
				bytecode.all[a] = O_ORAssignAndPop;
				bytecode.opcodes[o] = O_ORAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_ANDAssign )
			{
				bytecode.all[a] = O_ANDAssignAndPop;
				bytecode.opcodes[o] = O_ANDAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_XORAssign )
			{
				bytecode.all[a] = O_XORAssignAndPop;
				bytecode.opcodes[o] = O_XORAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_RightShiftAssign )
			{
				bytecode.all[a] = O_RightShiftAssignAndPop;
				bytecode.opcodes[o] = O_RightShiftAssignAndPop;
				return;
			}
			else if ( bytecode.opcodes[o] == O_LeftShiftAssign )
			{
				bytecode.all[a] = O_LeftShiftAssignAndPop;
				bytecode.opcodes[o] = O_LeftShiftAssignAndPop;
				return;
			}
		}
	}

	bytecode.all += opcode;
	bytecode.opcodes += opcode;
}

//------------------------------------------------------------------------------
// add a point to jump TO and return a list that should be filled with
// opcode + where to jump FROM
int WRCompilationContext::addRelativeJumpTarget( WRBytecode& bytecode )
{
	bytecode.jumpOffsetTargets.append().references.clear();
	return bytecode.jumpOffsetTargets.count() - 1;
}

//------------------------------------------------------------------------------
void WRCompilationContext::setRelativeJumpTarget( WRBytecode& bytecode, int relativeJumpTarget )
{
	bytecode.jumpOffsetTargets[relativeJumpTarget].offset = bytecode.all.size();
}

//------------------------------------------------------------------------------
// add a jump FROM with whatever opcode is supposed to do itw
void WRCompilationContext::addRelativeJumpSource( WRBytecode& bytecode, WROpcode opcode, int relativeJumpTarget )
{
	pushOpcode( bytecode, opcode );

	int offset = bytecode.all.size();
	switch( bytecode.opcodes[bytecode.opcodes.size() - 1] )
	{
		case O_GSCompareEQBZ:
		case O_LSCompareEQBZ:
		case O_GSCompareNEBZ:
		case O_LSCompareNEBZ:
		case O_GSCompareGEBZ:
		case O_LSCompareGEBZ:
		case O_GSCompareLEBZ:
		case O_LSCompareLEBZ:
		case O_GSCompareGTBZ:
		case O_LSCompareGTBZ:
		case O_GSCompareLTBZ:
		case O_LSCompareLTBZ:
		{
			--offset;
			break;
		}

		case O_LLCompareLTBZ:
		case O_LLCompareGTBZ:
		case O_LLCompareEQBZ:
		case O_LLCompareNEBZ:
		case O_GGCompareLTBZ:
		case O_GGCompareGTBZ:
		case O_GGCompareEQBZ:
		case O_GGCompareNEBZ:
		{
			offset -= 2;
			break;
		}

		default: break;
	}
	
	bytecode.jumpOffsetTargets[relativeJumpTarget].references.append() = offset;
	pushData( bytecode, "\t\t", 2 );
}

//------------------------------------------------------------------------------
void WRCompilationContext::resolveRelativeJumps( WRBytecode& bytecode )
{
	for( unsigned int j=0; j<bytecode.jumpOffsetTargets.count(); ++j )
	{
		for( unsigned int t=0; t<bytecode.jumpOffsetTargets[j].references.count(); ++t )
		{
			int16_t diff = bytecode.jumpOffsetTargets[j].offset - bytecode.jumpOffsetTargets[j].references[t];

			int offset = bytecode.jumpOffsetTargets[j].references[t];
			WROpcode o = (WROpcode)bytecode.all[offset - 1];

			switch( o )
			{
				case O_GSCompareEQBZ8:
				case O_LSCompareEQBZ8:
				case O_GSCompareNEBZ8:
				case O_LSCompareNEBZ8:
				case O_GSCompareGEBZ8:
				case O_LSCompareGEBZ8:
				case O_GSCompareLEBZ8:
				case O_LSCompareLEBZ8:
				case O_GSCompareGTBZ8:
				case O_LSCompareGTBZ8:
				case O_GSCompareLTBZ8:
				case O_LSCompareLTBZ8:
				case O_GSCompareEQBZ:
				case O_LSCompareEQBZ:
				case O_GSCompareNEBZ:
				case O_LSCompareNEBZ:
				case O_GSCompareGEBZ:
				case O_LSCompareGEBZ:
				case O_GSCompareLEBZ:
				case O_LSCompareLEBZ:
				case O_GSCompareGTBZ:
				case O_LSCompareGTBZ:
				case O_GSCompareLTBZ:
				case O_LSCompareLTBZ:
				{
					--diff; // these instructions are offset
					break;
				}

				case O_LLCompareLTBZ8:
				case O_LLCompareGTBZ8:
				case O_LLCompareEQBZ8:
				case O_LLCompareNEBZ8:
				case O_GGCompareLTBZ8:
				case O_GGCompareGTBZ8:
				case O_GGCompareEQBZ8:
				case O_GGCompareNEBZ8:
				case O_LLCompareLTBZ:
				case O_LLCompareGTBZ:
				case O_LLCompareEQBZ:
				case O_LLCompareNEBZ:
				case O_GGCompareLTBZ:
				case O_GGCompareGTBZ:
				case O_GGCompareEQBZ:
				case O_GGCompareNEBZ:
				{
					diff -= 2;
					break;
				}

				default:
					break;
			}

			
			if ( (diff < 128) && (diff > -129) )
			{
				switch( o )
				{
					case O_RelativeJump: *bytecode.all.p_str(offset - 1) = O_RelativeJump8; break;
					case O_BZ: *bytecode.all.p_str(offset - 1) = O_BZ8; break;

					case O_CompareBEQ: *bytecode.all.p_str(offset - 1) = O_CompareBEQ8; break;
					case O_CompareBNE: *bytecode.all.p_str(offset - 1) = O_CompareBNE8; break;
					case O_CompareBGE: *bytecode.all.p_str(offset - 1) = O_CompareBGE8; break;
					case O_CompareBLE: *bytecode.all.p_str(offset - 1) = O_CompareBLE8; break;
					case O_CompareBGT: *bytecode.all.p_str(offset - 1) = O_CompareBGT8; break;
					case O_CompareBLT: *bytecode.all.p_str(offset - 1) = O_CompareBLT8; break;
									   
					case O_GSCompareEQBZ: *bytecode.all.p_str(offset - 1) = O_GSCompareEQBZ8; ++offset; break;
					case O_LSCompareEQBZ: *bytecode.all.p_str(offset - 1) = O_LSCompareEQBZ8; ++offset; break;
					case O_GSCompareNEBZ: *bytecode.all.p_str(offset - 1) = O_GSCompareNEBZ8; ++offset; break;
					case O_LSCompareNEBZ: *bytecode.all.p_str(offset - 1) = O_LSCompareNEBZ8; ++offset; break;
					case O_GSCompareGEBZ: *bytecode.all.p_str(offset - 1) = O_GSCompareGEBZ8; ++offset; break;
					case O_LSCompareGEBZ: *bytecode.all.p_str(offset - 1) = O_LSCompareGEBZ8; ++offset; break;
					case O_GSCompareLEBZ: *bytecode.all.p_str(offset - 1) = O_GSCompareLEBZ8; ++offset; break;
					case O_LSCompareLEBZ: *bytecode.all.p_str(offset - 1) = O_LSCompareLEBZ8; ++offset; break;
					case O_GSCompareGTBZ: *bytecode.all.p_str(offset - 1) = O_GSCompareGTBZ8; ++offset; break;
					case O_LSCompareGTBZ: *bytecode.all.p_str(offset - 1) = O_LSCompareGTBZ8; ++offset; break;
					case O_GSCompareLTBZ: *bytecode.all.p_str(offset - 1) = O_GSCompareLTBZ8; ++offset; break;
					case O_LSCompareLTBZ: *bytecode.all.p_str(offset - 1) = O_LSCompareLTBZ8; ++offset; break;
										  
					case O_LLCompareLTBZ: *bytecode.all.p_str(offset - 1) = O_LLCompareLTBZ8; offset += 2; break;
					case O_LLCompareGTBZ: *bytecode.all.p_str(offset - 1) = O_LLCompareGTBZ8; offset += 2; break;
					case O_LLCompareEQBZ: *bytecode.all.p_str(offset - 1) = O_LLCompareEQBZ8; offset += 2; break;
					case O_LLCompareNEBZ: *bytecode.all.p_str(offset - 1) = O_LLCompareNEBZ8; offset += 2; break;
					case O_GGCompareLTBZ: *bytecode.all.p_str(offset - 1) = O_GGCompareLTBZ8; offset += 2; break;
					case O_GGCompareGTBZ: *bytecode.all.p_str(offset - 1) = O_GGCompareGTBZ8; offset += 2; break;
					case O_GGCompareEQBZ: *bytecode.all.p_str(offset - 1) = O_GGCompareEQBZ8; offset += 2; break;
					case O_GGCompareNEBZ: *bytecode.all.p_str(offset - 1) = O_GGCompareNEBZ8; offset += 2; break;

					case O_BLA: *bytecode.all.p_str(offset - 1) = O_BLA8; break;
					case O_BLO: *bytecode.all.p_str(offset - 1) = O_BLO8; break;
						
					// no work to be done, already visited
					case O_RelativeJump8:
					case O_BZ8:
					case O_BLA8:
					case O_BLO8:
					case O_CompareBLE8:
					case O_CompareBGE8:
					case O_CompareBGT8:
					case O_CompareBLT8:
					case O_CompareBEQ8:
					case O_CompareBNE8:
						break;
						
					case O_GSCompareEQBZ8:
					case O_LSCompareEQBZ8:
					case O_GSCompareNEBZ8:
					case O_LSCompareNEBZ8:
					case O_GSCompareGEBZ8:
					case O_LSCompareGEBZ8:
					case O_GSCompareLEBZ8:
					case O_LSCompareLEBZ8:
					case O_GSCompareGTBZ8:
					case O_LSCompareGTBZ8:
					case O_GSCompareLTBZ8:
					case O_LSCompareLTBZ8:
						++offset;
						break;

					case O_LLCompareLTBZ8:
					case O_LLCompareGTBZ8:
					case O_LLCompareEQBZ8:
					case O_LLCompareNEBZ8:
					case O_GGCompareLTBZ8:
					case O_GGCompareGTBZ8:
					case O_GGCompareEQBZ8:
					case O_GGCompareNEBZ8:
					{
						offset += 2;
						break;
					}

					default:
						m_err = WR_ERR_compiler_panic;
						return;
				}

				*bytecode.all.p_str(offset) = (int8_t)diff;
			}
			else
			{
				switch( o )
				{
					// check to see if any were pushed into 16-bit land
					// that were previously optimized
					case O_RelativeJump8: *bytecode.all.p_str(offset - 1) = O_RelativeJump; break;
					case O_BZ8: *bytecode.all.p_str(offset - 1) = O_BZ; break;
					case O_CompareBEQ8: *bytecode.all.p_str(offset - 1) = O_CompareBEQ; break;
					case O_CompareBNE8: *bytecode.all.p_str(offset - 1) = O_CompareBNE; break;
					case O_CompareBGE8: *bytecode.all.p_str(offset - 1) = O_CompareBGE; break;
					case O_CompareBLE8: *bytecode.all.p_str(offset - 1) = O_CompareBLE; break;
					case O_CompareBGT8: *bytecode.all.p_str(offset - 1) = O_CompareBGT; break;
					case O_CompareBLT8: *bytecode.all.p_str(offset - 1) = O_CompareBLT; break;
					case O_GSCompareEQBZ8: *bytecode.all.p_str(offset - 1) = O_GSCompareEQBZ; ++offset; break;
					case O_LSCompareEQBZ8: *bytecode.all.p_str(offset - 1) = O_LSCompareEQBZ; ++offset; break;
					case O_GSCompareNEBZ8: *bytecode.all.p_str(offset - 1) = O_GSCompareNEBZ; ++offset; break;
					case O_LSCompareNEBZ8: *bytecode.all.p_str(offset - 1) = O_LSCompareNEBZ; ++offset; break;
					case O_GSCompareGEBZ8: *bytecode.all.p_str(offset - 1) = O_GSCompareGEBZ; ++offset; break;
					case O_LSCompareGEBZ8: *bytecode.all.p_str(offset - 1) = O_LSCompareGEBZ; ++offset; break;
					case O_GSCompareLEBZ8: *bytecode.all.p_str(offset - 1) = O_GSCompareLEBZ; ++offset; break;
					case O_LSCompareLEBZ8: *bytecode.all.p_str(offset - 1) = O_LSCompareLEBZ; ++offset; break;
					case O_GSCompareGTBZ8: *bytecode.all.p_str(offset - 1) = O_GSCompareGTBZ; ++offset; break;
					case O_LSCompareGTBZ8: *bytecode.all.p_str(offset - 1) = O_LSCompareGTBZ; ++offset; break;
					case O_GSCompareLTBZ8: *bytecode.all.p_str(offset - 1) = O_GSCompareLTBZ; ++offset; break;
					case O_LSCompareLTBZ8: *bytecode.all.p_str(offset - 1) = O_LSCompareLTBZ; ++offset; break;

					case O_LLCompareLTBZ8: *bytecode.all.p_str(offset - 1) = O_LLCompareLTBZ; offset += 2; break;
					case O_LLCompareGTBZ8: *bytecode.all.p_str(offset - 1) = O_LLCompareGTBZ; offset += 2; break;
					case O_LLCompareEQBZ8: *bytecode.all.p_str(offset - 1) = O_LLCompareEQBZ; offset += 2; break;
					case O_LLCompareNEBZ8: *bytecode.all.p_str(offset - 1) = O_LLCompareNEBZ; offset += 2; break;
					case O_GGCompareLTBZ8: *bytecode.all.p_str(offset - 1) = O_GGCompareLTBZ; offset += 2; break;
					case O_GGCompareGTBZ8: *bytecode.all.p_str(offset - 1) = O_GGCompareGTBZ; offset += 2; break;
					case O_GGCompareEQBZ8: *bytecode.all.p_str(offset - 1) = O_GGCompareEQBZ; offset += 2; break;
					case O_GGCompareNEBZ8: *bytecode.all.p_str(offset - 1) = O_GGCompareNEBZ; offset += 2; break;

					case O_BLA8: *bytecode.all.p_str(offset - 1) = O_BLA; break;
					case O_BLO8: *bytecode.all.p_str(offset - 1) = O_BLO; break;

					 // no work to be done, already visited
					case O_RelativeJump:
					case O_BZ:
					case O_BLA:
					case O_BLO:
					case O_CompareBLE:
					case O_CompareBGE:
					case O_CompareBGT:
					case O_CompareBLT:
					case O_CompareBEQ:
					case O_CompareBNE:
						break;

					case O_GSCompareEQBZ:
					case O_LSCompareEQBZ:
					case O_GSCompareNEBZ:
					case O_LSCompareNEBZ:
					case O_GSCompareGEBZ:
					case O_LSCompareGEBZ:
					case O_GSCompareLEBZ:
					case O_LSCompareLEBZ:
					case O_GSCompareGTBZ:
					case O_LSCompareGTBZ:
					case O_GSCompareLTBZ:
					case O_LSCompareLTBZ:
						++offset;
						break;
					
					case O_LLCompareLTBZ:
					case O_LLCompareGTBZ:
					case O_LLCompareEQBZ:
					case O_LLCompareNEBZ:
					case O_GGCompareLTBZ:
					case O_GGCompareGTBZ:
					case O_GGCompareEQBZ:
					case O_GGCompareNEBZ:
					{
						offset += 2;
						break;
					}
						
					default:
						m_err = WR_ERR_compiler_panic;
						return;
				}
				
				pack16( diff, bytecode.all.p_str(offset) );
			}
		}
	}
}

//------------------------------------------------------------------------------
void WRCompilationContext::appendBytecode( WRBytecode& bytecode, WRBytecode& addMe )
{
	if ( bytecode.all.size() > 1
		 && bytecode.opcodes.size() > 0
		 && addMe.opcodes.size() == 1
		 && addMe.all.size() > 2
		 && addMe.opcodes[0] == O_IndexLiteral16
		 && bytecode.opcodes[bytecode.opcodes.size() - 1] == O_LoadFromLocal )
	{
		bytecode.all[bytecode.all.size()-2] = O_IndexLocalLiteral16;
		for( unsigned int i=1; i<addMe.all.size(); ++i )
		{
			bytecode.all += addMe.all[i];	
		}
		bytecode.opcodes.shave(1);
		bytecode.opcodes += O_IndexLocalLiteral16;
		return;
	}
	else if ( bytecode.all.size() > 1
			  && bytecode.opcodes.size() > 0
			  && addMe.opcodes.size() == 1
			  && addMe.all.size() > 2
			  && addMe.opcodes[0] == O_IndexLiteral16
			  && bytecode.opcodes[bytecode.opcodes.size() - 1] == O_LoadFromGlobal )
	{
		bytecode.all[bytecode.all.size()-2] = O_IndexGlobalLiteral16;
		for( unsigned int i=1; i<addMe.all.size(); ++i )
		{
			bytecode.all += addMe.all[i];	
		}
		bytecode.opcodes.shave(1);
		bytecode.opcodes += O_IndexGlobalLiteral16;
		return;
	}
	else if ( bytecode.all.size() > 1
			  && bytecode.opcodes.size() > 0
			  && addMe.opcodes.size() == 1
			  && addMe.all.size() > 1
			  && addMe.opcodes[0] == O_IndexLiteral8
			  && bytecode.opcodes[bytecode.opcodes.size() - 1] == O_LoadFromLocal )
	{
		bytecode.all[bytecode.all.size()-2] = O_IndexLocalLiteral8;
		for( unsigned int i=1; i<addMe.all.size(); ++i )
		{
			bytecode.all += addMe.all[i];	
		}
		bytecode.opcodes.shave(1);
		bytecode.opcodes += O_IndexLocalLiteral8;
		return;
	}
	else if ( bytecode.all.size() > 1
			  && bytecode.opcodes.size() > 0
			  && addMe.opcodes.size() == 1
			  && addMe.all.size() > 1
			  && addMe.opcodes[0] == O_IndexLiteral8
			  && bytecode.opcodes[bytecode.opcodes.size() - 1] == O_LoadFromGlobal )
	{
		bytecode.all[bytecode.all.size()-2] = O_IndexGlobalLiteral8;
		for( unsigned int i=1; i<addMe.all.size(); ++i )
		{
			bytecode.all += addMe.all[i];	
		}
		bytecode.opcodes.shave(1);
		bytecode.opcodes += O_IndexGlobalLiteral8;
		return;
	}
	else if ( bytecode.all.size() > 0
			  && addMe.opcodes.size() == 2
			  && addMe.all.size() == 3
			  && addMe.opcodes[1] == O_Index )
	{
		if ( bytecode.opcodes[bytecode.opcodes.size() - 1] == O_LoadFromLocal
			 && addMe.opcodes[0] == O_LoadFromLocal )
		{
			int a = bytecode.all.size() - 1;
			
			addMe.all[0] = bytecode.all[a];
			bytecode.all[a] = addMe.all[1];
			bytecode.all[a-1] = O_LLValues;
			bytecode.all += addMe.all[0];
			bytecode.all += O_IndexSkipLoad;
			
			bytecode.opcodes += O_LoadFromLocal;
			bytecode.opcodes += O_IndexSkipLoad;
			return;
		}
		else if ( bytecode.opcodes[bytecode.opcodes.size() - 1] == O_LoadFromGlobal
			 && addMe.opcodes[0] == O_LoadFromGlobal )
		{
			int a = bytecode.all.size() - 1;

			addMe.all[0] = bytecode.all[a];
			bytecode.all[a] = addMe.all[1];
			bytecode.all[a-1] = O_GGValues;
			bytecode.all += addMe.all[0];
			bytecode.all += O_IndexSkipLoad;

			bytecode.opcodes += O_LoadFromLocal;
			bytecode.opcodes += O_IndexSkipLoad;
			return;
		}
		else if ( bytecode.opcodes[bytecode.opcodes.size() - 1] == O_LoadFromLocal
				  && addMe.opcodes[0] == O_LoadFromGlobal )
		{
			int a = bytecode.all.size() - 1;

			addMe.all[0] = bytecode.all[a];
			bytecode.all[a] = addMe.all[1];
			bytecode.all[a-1] = O_GLValues;
			bytecode.all += addMe.all[0];
			bytecode.all += O_IndexSkipLoad;

			bytecode.opcodes += O_LoadFromLocal;
			bytecode.opcodes += O_IndexSkipLoad;
			return;
		}
		else if ( bytecode.opcodes[bytecode.opcodes.size() - 1] == O_LoadFromGlobal
				  && addMe.opcodes[0] == O_LoadFromLocal )
		{
			int a = bytecode.all.size() - 1;

			addMe.all[0] = bytecode.all[a];
			bytecode.all[a] = addMe.all[1];
			bytecode.all[a-1] = O_LGValues;
			bytecode.all += addMe.all[0];
			bytecode.all += O_IndexSkipLoad;

			bytecode.opcodes += O_LoadFromLocal;
			bytecode.opcodes += O_IndexSkipLoad;
			return;
		}
	}
	else if ( addMe.all.size() == 1 )
	{
		if ( addMe.opcodes[0] == O_HASH_PLACEHOLDER )
		{
			if ( bytecode.opcodes.size() > 1
				 && bytecode.opcodes[ bytecode.opcodes.size() - 2 ] == O_LiteralInt32
				 && bytecode.opcodes[ bytecode.opcodes.size() - 1 ] == O_LoadFromLocal )
			{
				int o = bytecode.all.size() - 7;

				bytecode.all[o] = O_LocalIndexHash;
				bytecode.all[o + 5] = bytecode.all[o + 4];
				bytecode.all[o + 4] = bytecode.all[o + 3];
				bytecode.all[o + 3] = bytecode.all[o + 2];
				bytecode.all[o + 2] = bytecode.all[o + 1];
				bytecode.all[o + 1] = bytecode.all[o + 6];

				bytecode.opcodes.shave(2);
				bytecode.all.shave(1);
			}
			else if (bytecode.opcodes.size() > 1
					 && bytecode.opcodes[ bytecode.opcodes.size() - 2 ] == O_LiteralInt32
					 && bytecode.opcodes[ bytecode.opcodes.size() - 1 ] == O_LoadFromGlobal )
			{
				int o = bytecode.all.size() - 7;

				bytecode.all[o] = O_GlobalIndexHash;
				bytecode.all[o + 5] = bytecode.all[o + 4];
				bytecode.all[o + 4] = bytecode.all[o + 3];
				bytecode.all[o + 3] = bytecode.all[o + 2];
				bytecode.all[o + 2] = bytecode.all[o + 1];
				bytecode.all[o + 1] = bytecode.all[o + 6];

				bytecode.opcodes.shave(2);
				bytecode.all.shave(1);
			}
			else if (bytecode.opcodes.size() > 1
					 && bytecode.opcodes[ bytecode.opcodes.size() - 2 ] == O_LiteralInt32
					 && bytecode.opcodes[ bytecode.opcodes.size() - 1 ] == O_StackSwap )
			{
				int a = bytecode.all.size() - 7;

				bytecode.all[a] = O_StackIndexHash;

				bytecode.opcodes.shave(2);
				bytecode.all.shave(2);

			}
			else
			{
				m_err = WR_ERR_compiler_panic;
			}

			return;
		}

		pushOpcode( bytecode, (WROpcode)addMe.opcodes[0] );
		return;
	}

	resolveRelativeJumps( addMe );

	// add the namespace, making sure to offset it into the new block properly
	for( unsigned int n=0; n<addMe.localSpace.count(); ++n )
	{
		unsigned int m = 0;
		for ( m=0; m<bytecode.localSpace.count(); ++m )
		{
			if ( bytecode.localSpace[m].hash == addMe.localSpace[n].hash )
			{
				for( unsigned int s=0; s<addMe.localSpace[n].references.count(); ++s )
				{
					bytecode.localSpace[m].references.append() = addMe.localSpace[n].references[s] + bytecode.all.size();
				}
				
				break;
			}
		}

		if ( m >= bytecode.localSpace.count() )
		{
			WRNamespaceLookup* space = &bytecode.localSpace.append();
			*space = addMe.localSpace[n];
			for( unsigned int s=0; s<space->references.count(); ++s )
			{
				space->references[s] += bytecode.all.size();
			}
		}
	}

	// add the function space, making sure to offset it into the new block properly
	for( unsigned int n=0; n<addMe.functionSpace.count(); ++n )
	{
		WRNamespaceLookup* space = &bytecode.functionSpace.append();
		*space = addMe.functionSpace[n];
		for( unsigned int s=0; s<space->references.count(); ++s )
		{
			space->references[s] += bytecode.all.size();
		}
	}

	// add the function space, making sure to offset it into the new block properly
	for( unsigned int u=0; u<addMe.unitObjectSpace.count(); ++u )
	{
		WRNamespaceLookup* space = &bytecode.unitObjectSpace.append();
		*space = addMe.unitObjectSpace[u];
		for( unsigned int s=0; s<space->references.count(); ++s )
		{
			space->references[s] += bytecode.all.size();
		}
	}

	bytecode.all += addMe.all;
	bytecode.opcodes += addMe.opcodes;
}

//------------------------------------------------------------------------------
void WRCompilationContext::pushLiteral( WRBytecode& bytecode, WRExpressionContext& context )
{
	WRValue& value = context.value;
	if ( value.type == WR_INT && value.i == 0 )
	{
		pushOpcode( bytecode, O_LiteralZero );
	}
	else if ( value.type == WR_INT )
	{
		if ( (value.i <= 127) && (value.i >= -128) )
		{
			pushOpcode( bytecode, O_LiteralInt8 );
			unsigned char be = (char)value.i;
			pushData( bytecode, &be, 1 );
		}
		else if ( (value.i <= 32767) && (value.i >= -32768) )
		{
			pushOpcode( bytecode, O_LiteralInt16 );
			unsigned char be = (char)(value.i>>8);
			pushData( bytecode, &be, 1 );
			be = (char)value.i;
			pushData( bytecode, &be, 1 );
		}
		else
		{
			pushOpcode( bytecode, O_LiteralInt32 );
			unsigned char data[4];
			pushData( bytecode, pack32(value.i, data), 4 );
		}
	}
	else if ( value.type == WR_COMPILER_LITERAL_STRING )
	{
		pushOpcode( bytecode, O_LiteralString );
		unsigned char data[2];
		int16_t be = context.literalString.size();
		pushData( bytecode, pack16(be, data), 2 );
		for( unsigned int i=0; i<context.literalString.size(); ++i )
		{
			pushData( bytecode, context.literalString.c_str(i), 1 );
		}
	}
	else
	{
		pushOpcode( bytecode, O_LiteralFloat );
		unsigned char data[4];
		int32_t be = value.i;
		pushData( bytecode, pack32(be, data), 4 );
	}
}

//------------------------------------------------------------------------------
void WRCompilationContext::addLocalSpaceLoad( WRBytecode& bytecode, WRstr& token, bool addOnly )
{
	if ( m_unitTop == 0 )
	{
		assert( !addOnly );
		addGlobalSpaceLoad( bytecode, token );
		return;
	}
	
	uint32_t hash = wr_hash( token, token.size() );

	unsigned int i=0;
		
	for( ; i<bytecode.localSpace.count(); ++i )
	{
		if ( bytecode.localSpace[i].hash == hash )
		{
			break;
		}
	}

	if ( i >= bytecode.localSpace.count() && !addOnly )
	{
		for (unsigned int j = 0; j < m_units[0].bytecode.localSpace.count(); ++j)
		{
			if (m_units[0].bytecode.localSpace[j].hash == hash)
			{
				pushOpcode(bytecode, O_LoadFromGlobal);
				unsigned char c = j;
				pushData(bytecode, &c, 1);
				return;
			}
		}
	}

	bytecode.localSpace[i].hash = hash;

	if ( !addOnly )
	{
		pushOpcode( bytecode, O_LoadFromLocal );
		unsigned char c = i;
		pushData( bytecode, &c, 1 );
	}
}

//------------------------------------------------------------------------------
void WRCompilationContext::addGlobalSpaceLoad( WRBytecode& bytecode, WRstr& token )
{
	uint32_t hash;
	WRstr t2;
	if ( token[0] == ':' && token[1] == ':' )
	{
		t2 = token;
	}
	else
	{
		t2.format( "::%s", token.c_str() );
	}
	hash = wr_hash( t2, t2.size() );

	unsigned int i=0;

	for( ; i<m_units[0].bytecode.localSpace.count(); ++i )
	{
		if ( m_units[0].bytecode.localSpace[i].hash == hash )
		{
			break;
		}
	}
	
	m_units[0].bytecode.localSpace[i].hash = hash;

	pushOpcode( bytecode, O_LoadFromGlobal );
	unsigned char c = i;
	pushData( bytecode, &c, 1 );
}

//------------------------------------------------------------------------------
void WRCompilationContext::loadExpressionContext( WRExpression& expression, int depth, int operation )
{
	if ( operation > 0
		 && expression.context[operation].operation
		 && expression.context[operation].operation->opcode == O_HASH_PLACEHOLDER
		 && depth > operation )
	{
		unsigned char buf[4];
		pack32( wr_hash( expression.context[depth].token,
						 expression.context[depth].token.size()),
				buf );
		pushOpcode( expression.bytecode, O_LiteralInt32 );
		pushData( expression.bytecode, buf, 4 );
	}
	else
	{
		switch( expression.context[depth].type )
		{
			case EXTYPE_LITERAL:
			{
				pushLiteral( expression.bytecode, expression.context[depth] );
				break;
			}

			case EXTYPE_LABEL:
			{
				if ( expression.context[depth].global )
				{
					addGlobalSpaceLoad( expression.bytecode,
										expression.context[depth].token );
				}
				else
				{
					addLocalSpaceLoad( expression.bytecode,
									   expression.context[depth].token );
				}
				break;
			}

			case EXTYPE_BYTECODE_RESULT:
			{
				appendBytecode( expression.bytecode,
								expression.context[depth].bytecode );
				break;
			}

			case EXTYPE_RESOLVED:
			case EXTYPE_OPERATION:
			default:
			{
				break;
			}
		}
	}

	expression.pushToStack( depth ); // make sure stack knows something got loaded on top of it
	
	expression.context[depth].type = EXTYPE_RESOLVED; // this slot is now a resolved value sitting on the stack
}

//------------------------------------------------------------------------------
void WRExpression::swapWithTop( int stackPosition, bool addOpcodes )
{
	if ( stackPosition == 0 )
	{
		return;
	}
	
	unsigned int currentTop = -1;
	unsigned int swapWith = -1;
	for( unsigned int i=0; i<context.count(); ++i )
	{
		if ( context[i].stackPosition == stackPosition )
		{
			swapWith = i;
		}
		
		if ( context[i].stackPosition == 0 )
		{
			currentTop = i;
		}
	}

	assert( (currentTop != (unsigned int)-1) && (swapWith != (unsigned int)-1) );

	if ( addOpcodes )
	{
		unsigned char pos = stackPosition + 1;
		WRCompilationContext::pushOpcode( bytecode, O_StackSwap );
		WRCompilationContext::pushData( bytecode, &pos, 1 );
	}
	context[currentTop].stackPosition = stackPosition;
	context[swapWith].stackPosition = 0;
}

//------------------------------------------------------------------------------
void WRCompilationContext::resolveExpression( WRExpression& expression )
{
	if ( expression.context.count() == 1 ) // single expression is trivial to resolve, load it!
	{
		loadExpressionContext( expression, 0, 0 );
		return;
	}

	unsigned int resolves = 1;
	unsigned int startedWith = expression.context.count();

	for( int p=0; p<c_highestPrecedence && expression.context.count() > 1; ++p )
	{
		// left to right operations
		for( int o=0; (unsigned int)o < expression.context.count(); ++o )
		{
			if ( (expression.context[o].type != EXTYPE_OPERATION)
				 || expression.context[o].operation->precedence > p
				 || !expression.context[o].operation->leftToRight )
			{
				continue;
			}

			resolves += resolveExpressionEx( expression, o, p );
			if (m_err)
			{
				return;
			}
			o = (unsigned int)-1;
		}

		// right to left operations
		for( int o = expression.context.count() - 1; o >= 0; --o )
		{
			if ( (expression.context[o].type != EXTYPE_OPERATION)
				 || expression.context[o].operation->precedence > p
				 || expression.context[o].operation->leftToRight )
			{
				continue;
			}

			resolves += resolveExpressionEx( expression, o, p );
			if (m_err)
			{
				return;
			}
			o = expression.context.count();
		}
	}

	if ( startedWith && (resolves != startedWith) )
	{
		m_err = WR_ERR_bad_expression;
		return;
	}
}

//------------------------------------------------------------------------------
unsigned int WRCompilationContext::resolveExpressionEx( WRExpression& expression, int o, int p )
{
	unsigned int ret = 0;
	switch( expression.context[o].operation->type )
	{
		case WR_OPER_PRE:
		{
			ret = 1;

			if ( expression.context[o + 1].stackPosition == -1 )
			{
				loadExpressionContext( expression, o + 1, 0 ); // load argument
			}
			else if ( expression.context[o + 1].stackPosition != 0 )
			{
				expression.swapWithTop( expression.context[o + 1].stackPosition );
			}

			appendBytecode( expression.bytecode, expression.context[o].bytecode );  // apply operator
			expression.context.remove( o, 1 ); // knock off operator
			expression.pushToStack(o);
			break;
		}

		case WR_OPER_BINARY_COMMUTE:
		{
			// for operations like + and * the operands can be in any
			// order, so don't go to any great lengths to shift the stack
			// around for them

			ret = 2;

			if( o == 0 )
			{
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			int second = o + 1; // push first
			int first = o - 1;  // push second
			// so
			// 1 - first
			// [oper]
			// 0 - second

			if ( expression.context[second].stackPosition == -1 )
			{
				if ( expression.context[first].stackPosition == -1 )
				{
					loadExpressionContext( expression, first, o ); 
				}
				else if ( expression.context[first].stackPosition != 0 )
				{
					// otherwise swap first to the top and load the second
					expression.swapWithTop( expression.context[first].stackPosition );
				}

				loadExpressionContext( expression, second, o );
			}
			else if ( expression.context[first].stackPosition == -1 )
			{
				if ( expression.context[second].stackPosition != 0 )
				{
					expression.swapWithTop( expression.context[second].stackPosition );
				}

				// just load the second to top
				loadExpressionContext( expression, first, o );
			}
			else if ( expression.context[second].stackPosition == 1 )
			{
				expression.swapWithTop( expression.context[first].stackPosition );
			}
			else if ( expression.context[first].stackPosition == 1 )
			{
				expression.swapWithTop( expression.context[second].stackPosition );
			}
			else
			{
				// first and second are both loaded but neither
				// is in the correct position

				WRCompilationContext::pushOpcode( expression.bytecode, O_SwapTwoToTop );
				unsigned char pos = expression.context[first].stackPosition + 1;
				WRCompilationContext::pushData( expression.bytecode, &pos, 1 );
				pos = expression.context[second].stackPosition + 1;
				WRCompilationContext::pushData( expression.bytecode, &pos, 1 );

				expression.swapWithTop( expression.context[second].stackPosition, false );
				expression.swapWithTop( 1, false );
				expression.swapWithTop( expression.context[first].stackPosition, false );
			}

			appendBytecode( expression.bytecode, expression.context[o].bytecode ); // apply operator

			expression.context.remove( o - 1, 2 ); // knock off operator and arg
			expression.pushToStack(o - 1);

			break;
		}

		case WR_OPER_BINARY:
		{
			ret = 2;

			if( o == 0 )
			{
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			int second = o + 1; // push first
			int first = o - 1;  // push second
			bool useAlt = false;
			// so
			// 1 - first
			// [oper]
			// 0 - second

			if ( expression.context[second].stackPosition == -1 )
			{
				if ( expression.context[first].stackPosition == -1 )
				{
					loadExpressionContext( expression, second, o ); // nope, grab 'em
					loadExpressionContext( expression, first, o ); 
				}
				else
				{
					// first is in the stack somewhere, we need
					// to swap it to the top, then load, then
					// swap top values
					expression.swapWithTop( expression.context[first].stackPosition );

					loadExpressionContext( expression, second, o );

					if ( expression.context[o].operation->alt == O_LAST )
					{
						expression.swapWithTop( 1 );
					}
					else // otherwise top shuffle is NOT required because we have an equal-but-opposite operation
					{
						useAlt = true;
					}
				}
			}
			else if ( expression.context[first].stackPosition == -1 )
			{
				// perfect, swap up the second, then load the
				// first

				expression.swapWithTop( expression.context[second].stackPosition );

				loadExpressionContext( expression, first, o );
			}
			else if ( expression.context[second].stackPosition == 1 )
			{
				// second is already where its supposed to be,
				// swap first to top if it's not there already
				if ( expression.context[first].stackPosition != 0 )
				{
					expression.swapWithTop( expression.context[first].stackPosition );
				}
			}
			else if ( expression.context[second].stackPosition == 0 )
			{
				// second is on top of the stack, swap with
				// next level down then swap first up

				WRCompilationContext::pushOpcode( expression.bytecode, O_SwapTwoToTop );
				unsigned char pos = expression.context[first].stackPosition + 1;
				WRCompilationContext::pushData( expression.bytecode, &pos, 1 );
				pos = 2;
				WRCompilationContext::pushData( expression.bytecode, &pos, 1 );

				expression.swapWithTop( 1, false );
				expression.swapWithTop( expression.context[first].stackPosition, false );
			}
			else
			{
				// first and second are both loaded but neither is in the correct position

				WRCompilationContext::pushOpcode( expression.bytecode, O_SwapTwoToTop );
				unsigned char pos = expression.context[first].stackPosition + 1;
				WRCompilationContext::pushData( expression.bytecode, &pos, 1 );
				pos = expression.context[second].stackPosition + 1;
				WRCompilationContext::pushData( expression.bytecode, &pos, 1 );

				expression.swapWithTop( expression.context[second].stackPosition, false );
				expression.swapWithTop( 1, false );
				expression.swapWithTop( expression.context[first].stackPosition, false );
			}

			if ( useAlt )
			{
				pushOpcode( expression.bytecode, expression.context[o].operation->alt );
			}
			else
			{
				appendBytecode( expression.bytecode, expression.context[o].bytecode ); // apply operator
			}

			expression.context.remove( o - 1, 2 ); // knock off operator and arg
			expression.pushToStack(o - 1);

			break;
		}

		case WR_OPER_POST:
		{
			ret = 1;

			if( o == 0 )
			{
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			if ( expression.context[o - 1].stackPosition == -1 )
			{
				loadExpressionContext( expression, o - 1, o ); // load argument
			}
			else if ( expression.context[o-+ 1].stackPosition != 0 )
			{
				expression.swapWithTop( expression.context[o - 1].stackPosition );
			}

			appendBytecode( expression.bytecode, expression.context[o].bytecode );
			expression.context.remove( o, 1 ); // knock off operator
			expression.pushToStack( o - 1 );

			break;
		}
	}
	return ret;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::operatorFound( WRstr& token, WRarray<WRExpressionContext>& context, int depth )
{
	for( int i=0; c_operations[i].token; i++ )
	{
		if ( token == c_operations[i].token )
		{
			if ( c_operations[i].type == WR_OPER_PRE
				 && depth > 0
				 && (context[depth-1].type != EXTYPE_OPERATION 
					 || (context[depth - 1].type == EXTYPE_OPERATION &&
						 (context[depth - 1].operation->type != WR_OPER_BINARY && context[depth - 1].operation->type != WR_OPER_BINARY_COMMUTE))) )
			{
				continue;
			}
			
			context[depth].operation = c_operations + i;
			context[depth].type = EXTYPE_OPERATION;

			pushOpcode( context[depth].bytecode, c_operations[i].opcode );

			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::parseCallFunction( WRExpression& expression, WRstr functionName, int depth, bool parseArguments )
{
	WRstr prefix = expression.context[depth].prefix;

	expression.context[depth].reset();
	expression.context[depth].type = EXTYPE_BYTECODE_RESULT;

	unsigned char argsPushed = 0;

	if ( parseArguments )
	{
		WRstr& token2 = expression.context[depth].token;
		WRValue& value2 = expression.context[depth].value;

		for(;;)
		{
			if ( !getToken(expression.context[depth]) )
			{
				m_err = WR_ERR_unexpected_EOF;
				return 0;
			}

			if ( token2 == ")" )
			{
				break;
			}

			++argsPushed;

			WRExpression nex( expression.bytecode.localSpace );
			nex.context[0].token = token2;
			nex.context[0].value = value2;
			m_loadedToken = token2;
			m_loadedValue = value2;

			char end = parseExpression( nex );

			appendBytecode( expression.context[depth].bytecode, nex.bytecode );

			if ( end == ')' )
			{
				break;
			}
			else if ( end != ',' )
			{
				m_err = WR_ERR_unexpected_token;
				return 0;
			}
		}
	}

	if ( prefix.size() )
	{
		prefix += "::";
		prefix += functionName;

		unsigned char buf[4];
		pack32( wr_hashStr(prefix), buf );

		pushOpcode( expression.context[depth].bytecode, O_CallLibFunction );
		pushData( expression.context[depth].bytecode, &argsPushed, 1 ); // arg #
		pushData( expression.context[depth].bytecode, buf, 4 ); // hash id

		// normal lib will copy down the result, otherwise
		// ignore it, it will be AFTER args
	}
	else
	{
		// push the number of args
		uint32_t hash = wr_hashStr( functionName );

		unsigned int i=0;
		for( ; i<expression.context[depth].bytecode.functionSpace.count(); ++i )
		{
			if ( expression.context[depth].bytecode.functionSpace[i].hash == hash )
			{
				break;
			}
		}

		expression.context[depth].bytecode.functionSpace[i].references.append() = getBytecodePosition( expression.context[depth].bytecode );
		expression.context[depth].bytecode.functionSpace[i].hash = hash;

		pushOpcode( expression.context[depth].bytecode, O_FUNCTION_CALL_PLACEHOLDER );

		pushData( expression.context[depth].bytecode, &argsPushed, 1 );
		pushData( expression.context[depth].bytecode, "0123", 4 ); // TBD opcode plus index, OR hash if index was not found

		// hash will copydown result same as lib, unless
		// copy/pop which case does nothing

		// normal call does nothing special, to preserve
		// the return value a call to copy-down must be
		// inserted
	}

	return true;
}

//------------------------------------------------------------------------------
char WRCompilationContext::parseExpression( WRExpression& expression )
{
	int depth = 0;
	char end = 0;

	for(;;)
	{
		WRValue& value = expression.context[depth].value;
		WRstr& token = expression.context[depth].token;

		expression.context[depth].bytecode.clear();
		expression.context[depth].setLocalSpace( expression.bytecode.localSpace );
		if ( !getToken(expression.context[depth]) )
		{
			return 0;
		}

		if ( value.type != WR_REF )
		{
			if ( (depth > 0) 
				 && ((expression.context[depth - 1].type == EXTYPE_LABEL) 
					 || (expression.context[depth - 1].type == EXTYPE_LITERAL)) )
			{
				// two labels/literals cannot follow each other
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			// it's a literal
			expression.context[depth].type = EXTYPE_LITERAL;
			if ( value.type == WR_COMPILER_LITERAL_STRING )
			{
				expression.context[depth].literalString = *(WRstr*)value.p;
			}
			
			++depth;
			continue;
		}


		if ( token == ";" || token == ")" || token == "}" || token == "," || token == "]" )
		{
			end = token[0];
			break;
		}

		if ( token == "{" )
		{
			// it's an initializer

			if ( (depth < 2) || 
					(expression.context[depth - 1].operation
					&& expression.context[ depth - 1 ].operation->opcode != O_Assign) )
			{
				m_err = WR_ERR_unexpected_token;
				return 0;
			}

			if ( expression.context[ depth - 2 ].operation->opcode != O_Index )
			{
				m_err = WR_ERR_unexpected_token;
				return 0;
			}

			expression.context.remove( depth - 1, 1 ); // knock off the equate
			depth--;

			WRstr& token2 = expression.context[depth].token;
			WRValue& value2 = expression.context[depth].value;

			int16_t initializer = 0;

			for(;;)
			{
				if ( !getToken(expression.context[depth]) )
				{
					m_err = WR_ERR_unexpected_EOF;
					return 0;
				}

				if ( token2 == "}" )
				{
					break;
				}

				WRExpression nex( expression.bytecode.localSpace );
				nex.context[0].token = token2;
				nex.context[0].value = value2;
				m_loadedToken = token2;
				m_loadedValue = value2;

				char end = parseExpression( nex );

				appendBytecode( expression.context[depth-1].bytecode, nex.bytecode );
				pushOpcode( expression.context[depth-1].bytecode, O_AssignToArrayAndPop );
				unsigned char data[2];
				pack16( initializer, data );
				pushData( expression.context[depth-1].bytecode, data, 2 );

				++initializer;

				if ( end == '}' )
				{
					break;
				}
				else if ( end != ',' )
				{
					m_err = WR_ERR_unexpected_token;
					return 0;
				}
			}

			if ( !getToken(expression.context[depth]) || token2 != ";" )
			{
				m_err = WR_ERR_unexpected_EOF;
				return 0;
			}

			m_loadedToken = token2;
			m_loadedValue = value2;

			// ++depth;  DO NOT DO THIS. let this operation fall off,
			//           its code was added to the previous post operation
			
			continue;
		}
		
		if ( operatorFound(token, expression.context, depth) )
		{
			++depth;
			continue;
		}
		
		if ( token == "new" )
		{
			WRstr& token2 = expression.context[depth].token;
			WRValue& value2 = expression.context[depth].value;

			bool isGlobal;
			WRstr prefix;

			if ( !getToken(expression.context[depth]) )
			{
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			if ( !isValidLabel(token2, isGlobal, prefix) )
			{
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			WRstr functionName = token2;
			uint32_t hash = wr_hashStr( functionName );
			
			if ( !getToken(expression.context[depth]) )
			{
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			if ( token2 == ";" )
			{
				if ( !parseCallFunction(expression, functionName, depth, false) )
				{
					return 0;
				}

				m_loadedToken = ";";
				m_loadedValue.p2 = INIT_AS_REF;
			}
			else if ( token2 == "(" )
			{
				if ( !parseCallFunction(expression, functionName, depth, true) )
				{
					return 0;
				}

				if ( !getToken(expression.context[depth]) )
				{
					m_err = WR_ERR_unexpected_EOF;
					return 0;
				}

				if ( token2 != "{" )
				{
					m_loadedToken = token2;
					m_loadedValue = value2;
				}
			}
			else if (token2 == "{")
			{
				if ( !parseCallFunction(expression, functionName, depth, false) )
				{
					return 0;
				}
				token2 = "{";
			}
			else if ( token2 != "{" )
			{
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			unsigned int i=0;
			for( ; i<expression.context[depth].bytecode.unitObjectSpace.count(); ++i )
			{
				if ( expression.context[depth].bytecode.unitObjectSpace[i].hash == hash )
				{
					break;
				}
			}

			pushOpcode( expression.context[depth].bytecode, O_NewHashTable );

			expression.context[depth].bytecode.unitObjectSpace[i].references.append() = getBytecodePosition( expression.context[depth].bytecode );
			expression.context[depth].bytecode.unitObjectSpace[i].hash = hash;

			pushData( expression.context[depth].bytecode, "\0\0", 2 );

			expression.context[depth].type = EXTYPE_BYTECODE_RESULT;

			if ( token2 == "{" )
			{
				unsigned char offset = 0;
				for(;;)
				{
					WRExpression nex( expression.bytecode.localSpace );
					nex.context[0].token = token2;
					nex.context[0].value = value2;
					char end = parseExpression( nex );

					if ( nex.bytecode.all.size() )
					{
						appendBytecode( expression.context[depth].bytecode, nex.bytecode );
						pushOpcode( expression.context[depth].bytecode, O_AssignToHashTableByOffset );
						pushData( expression.context[depth].bytecode, &offset, 1 );
					}

					++offset;

					if ( end == '}' )
					{
						break;
					}
					else if ( end != ',' )
					{
						m_err = WR_ERR_unexpected_token;
						return 0;
					}
				}
			}

			++depth;
			continue;
		}

		if ( token == "(" )
		{
			// might be cast, call or sub-expression
			
			if ( (depth > 0) && expression.context[depth - 1].type == EXTYPE_LABEL )
			{
				// always only a call

				--depth;
				if ( !parseCallFunction(expression, expression.context[depth].token, depth, true) )
				{
					return 0;
				}
			}
			else if ( !getToken(expression.context[depth]) )
			{
				m_err = WR_ERR_unexpected_EOF;
				return 0;
			}
			else if ( token == "int" )
			{
				if ( !getToken(expression.context[depth], ")") )
				{
					m_err = WR_ERR_bad_expression;
					return 0;
				}

				WRstr t( "@i" );
				operatorFound( t, expression.context, depth );
			}
			else if ( token == "float" )
			{
				if ( !getToken(expression.context[depth], ")") )
				{
					m_err = WR_ERR_bad_expression;
					return 0;
				}

				WRstr t( "@f" );
				operatorFound( t, expression.context, depth );
			}
			else if (depth < 0)
			{
				m_err = WR_ERR_bad_expression;
				return 0;
			}
			else
			{
				WRExpression nex( expression.bytecode.localSpace );
				nex.context[0].token = token;
				nex.context[0].value = value;
				m_loadedToken = token;
				m_loadedValue = value;
				if ( parseExpression(nex) != ')' )
				{
					m_err = WR_ERR_unexpected_token;
					return 0;
				}

				expression.context[depth].type = EXTYPE_BYTECODE_RESULT;
				expression.context[depth].bytecode = nex.bytecode;
			}

			++depth;
			continue;
		}

		if ( token == "[" )
		{
			if ( depth == 0
				 || (expression.context[depth - 1].type != EXTYPE_LABEL
					 && expression.context[depth - 1].type != EXTYPE_BYTECODE_RESULT) )
			{
				m_err = WR_ERR_unexpected_EOF;
				return 0;
			}


			WRExpression nex( expression.bytecode.localSpace );
			nex.context[0].token = token;
			nex.context[0].value = value;

			if ( parseExpression( nex ) != ']' )
			{
				m_err = WR_ERR_unexpected_EOF;
				return 0;
			}
			
			expression.context[depth].bytecode = nex.bytecode;

			WRstr t( "@[]" );
			operatorFound( t, expression.context, depth );

			++depth;
			continue;
		}

		bool isGlobal;
		WRstr prefix;
		if ( isValidLabel(token, isGlobal, prefix) )
		{
			if ( (depth > 0) 
				&& ((expression.context[depth - 1].type == EXTYPE_LABEL) 
					|| (expression.context[depth - 1].type == EXTYPE_LITERAL)) )
			{
				// two labels/literals cannot follow each other
				m_err = WR_ERR_bad_expression;
				return 0;
			}

			expression.context[depth].type = EXTYPE_LABEL;
			expression.context[depth].global = isGlobal;
			expression.context[depth].prefix = prefix;
			++depth;

			continue;
		}

		m_err = WR_ERR_bad_expression;
		return 0;
	}

	expression.context.setCount( expression.context.count() - 1 );

	resolveExpression( expression );

	return end;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::parseUnit()
{
	int previousIndex = m_unitTop;
	m_unitTop = m_units.count();
	
	bool isGlobal;

	WRExpressionContext ex;
	WRstr& token = ex.token;
	WRstr prefix;
	
	// get the function name
	if ( !getToken(ex)
		 || !isValidLabel(token, isGlobal, prefix)
		 || isGlobal )
	{
		m_err = WR_ERR_bad_label;
		return false;
	}

	m_units[m_unitTop].hash = wr_hash( token, token.size() );
	
	// get the function name
	if ( getToken(ex, "(") )
	{
		for(;;)
		{
			if ( !getToken(ex) )
			{
				m_err = WR_ERR_unexpected_EOF;
				return false;
			}
			
			if ( token == ")" )
			{
				break;
			}
			
			if ( !isValidLabel(token, isGlobal, prefix) || isGlobal )
			{
				m_err = WR_ERR_bad_label;
				return false;
			}

			++m_units[m_unitTop].arguments;

			// register the argument on the hash stack
			addLocalSpaceLoad( m_units[m_unitTop].bytecode, token, true );

			if ( !getToken(ex) )
			{
				m_err = WR_ERR_unexpected_EOF;
				return false;
			}

			if ( token == ")" )
			{
				break;
			}

			if ( token != "," )
			{
				m_err = WR_ERR_unexpected_token;
				return false;
			}
		}

		if ( !getToken(ex, "{") )
		{
			m_err = WR_ERR_unexpected_token;
			return false;
		}
	}
	else if ( token != "{" )
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}
		
	bool returnCalled;
	parseStatement( m_unitTop, '}', returnCalled, O_Return );

	if ( !returnCalled )
	{
		pushOpcode( m_units[m_unitTop].bytecode, O_LiteralZero );
		pushOpcode( m_units[m_unitTop].bytecode, O_Return );
	}

	m_unitTop = previousIndex;

	return true;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::parseWhile( bool& returnCalled, WROpcode opcodeToReturn )
{
	WRExpressionContext ex;
	WRstr& token = ex.token;
	WRValue& value = ex.value;

	if ( !getToken(ex, "(") )
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}

	if ( !getToken(ex) )
	{
		m_err = WR_ERR_unexpected_EOF;
		return false;
	}

	WRExpression nex( m_units[m_unitTop].bytecode.localSpace );
	nex.context[0].token = token;
	nex.context[0].value = value;
	m_loadedToken = token;
	m_loadedValue = value;

	if ( parseExpression(nex) != ')' )
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}

	*m_continueTargets.push() = addRelativeJumpTarget( m_units[m_unitTop].bytecode );
	setRelativeJumpTarget(m_units[m_unitTop].bytecode, *m_continueTargets.tail() );

	appendBytecode( m_units[m_unitTop].bytecode, nex.bytecode );

	*m_breakTargets.push() = addRelativeJumpTarget( m_units[m_unitTop].bytecode );

	addRelativeJumpSource( m_units[m_unitTop].bytecode, O_BZ, *m_breakTargets.tail() );

	if ( !parseStatement(m_unitTop, ';', returnCalled, opcodeToReturn) )
	{
		return false;
	}
	
	addRelativeJumpSource( m_units[m_unitTop].bytecode, O_RelativeJump, *m_continueTargets.tail() );
	setRelativeJumpTarget( m_units[m_unitTop].bytecode, *m_breakTargets.tail() );

	m_continueTargets.pop();
	m_breakTargets.pop();
	
	resolveRelativeJumps( m_units[m_unitTop].bytecode );
	
	return true;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::parseDoWhile( bool& returnCalled, WROpcode opcodeToReturn )
{
	*m_continueTargets.push() = addRelativeJumpTarget( m_units[m_unitTop].bytecode );
	*m_breakTargets.push() = addRelativeJumpTarget( m_units[m_unitTop].bytecode );

	int jumpToTop = addRelativeJumpTarget( m_units[m_unitTop].bytecode );
	setRelativeJumpTarget( m_units[m_unitTop].bytecode, jumpToTop );
	
	if ( !parseStatement(m_unitTop, ';', returnCalled, opcodeToReturn) )
	{
		return false;
	}

	setRelativeJumpTarget(m_units[m_unitTop].bytecode, *m_continueTargets.tail() );

	WRExpressionContext ex;
	WRstr& token = ex.token;
	WRValue& value = ex.value;

	if ( !getToken(ex) )
	{
		m_err = WR_ERR_unexpected_EOF;
		return false;
	}

	if ( token != "while" )
	{
		m_err = WR_ERR_expected_while;
		return false;
	}

	if ( !getToken(ex, "(") )
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}

	WRExpression nex( m_units[m_unitTop].bytecode.localSpace );
	nex.context[0].token = token;
	nex.context[0].value = value;
	//m_loadedToken = token;
	//m_loadedValue = value;

	if ( parseExpression(nex) != ')' )
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}

	if (!getToken(ex, ";"))
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}
	setRelativeJumpTarget(m_units[m_unitTop].bytecode, *m_continueTargets.tail() );

	appendBytecode( m_units[m_unitTop].bytecode, nex.bytecode );

	addRelativeJumpSource( m_units[m_unitTop].bytecode, O_BZ, *m_breakTargets.tail() );
	addRelativeJumpSource( m_units[m_unitTop].bytecode, O_RelativeJump, jumpToTop );
	
	setRelativeJumpTarget( m_units[m_unitTop].bytecode, *m_breakTargets.tail() );

	m_continueTargets.pop();
	m_breakTargets.pop();

	resolveRelativeJumps( m_units[m_unitTop].bytecode );

	return true;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::parseForLoop( bool& returnCalled, WROpcode opcodeToReturn )
{
	WRExpressionContext ex;
	WRstr& token = ex.token;
	WRValue& value = ex.value;

	if ( !getToken(ex, "(") )
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}

	// create the continue and break reference points
	*m_continueTargets.push() = addRelativeJumpTarget( m_units[m_unitTop].bytecode );
	*m_breakTargets.push() = addRelativeJumpTarget( m_units[m_unitTop].bytecode );

/*


[setup]
<- condition point
[condition]
-> false jump break
[code]
<- continue point
[post code]
-> always jump condition
<- break point

*/
	
	// [setup]
	for(;;)
	{
		if ( !getToken(ex) )
		{
			m_err = WR_ERR_unexpected_EOF;
			return false;
		}

		if ( token == ";" )
		{
			break;
		}

		WRExpression nex( m_units[m_unitTop].bytecode.localSpace );
		nex.context[0].token = token;
		nex.context[0].value = value;
		m_loadedToken = token;
		m_loadedValue = value;

		char end = parseExpression( nex );
		pushOpcode( nex.bytecode, O_PopOne );

		appendBytecode( m_units[m_unitTop].bytecode, nex.bytecode );
		
		if ( end == ';' )
		{
			break;
		}
		else if ( end != ',' )
		{
			m_err = WR_ERR_unexpected_token;
			return 0;
		}
	}

	// <- condition point
	int conditionPoint = addRelativeJumpTarget( m_units[m_unitTop].bytecode );
	setRelativeJumpTarget( m_units[m_unitTop].bytecode, conditionPoint );

	
	if ( !getToken(ex) )
	{
		m_err = WR_ERR_unexpected_EOF;
		return false;
	}

	// [ condition ]
	if ( token != ";" )
	{
		WRExpression nex( m_units[m_unitTop].bytecode.localSpace );
		nex.context[0].token = token;
		nex.context[0].value = value;
		m_loadedToken = token;
		m_loadedValue = value;
		

		if ( parseExpression( nex ) != ';' )
		{
			m_err = WR_ERR_unexpected_token;
			return false;
		}
		
		appendBytecode( m_units[m_unitTop].bytecode, nex.bytecode );

		// -> false jump break
		addRelativeJumpSource( m_units[m_unitTop].bytecode, O_BZ, *m_breakTargets.tail() );
	}


	WRExpression post( m_units[m_unitTop].bytecode.localSpace );

	// [ post code ]
	for(;;)
	{
		if ( !getToken(ex) )
		{
			m_err = WR_ERR_unexpected_EOF;
			return false;
		}

		if ( token == ")" )
		{
			break;
		}

		WRExpression nex( m_units[m_unitTop].bytecode.localSpace );
		nex.context[0].token = token;
		nex.context[0].value = value;
		m_loadedToken = token;
		m_loadedValue = value;

		char end = parseExpression( nex );
		pushOpcode( nex.bytecode, O_PopOne );

		appendBytecode( post.bytecode, nex.bytecode );

		if ( end == ')' )
		{
			break;
		}
		else if ( end != ',' )
		{
			m_err = WR_ERR_unexpected_token;
			return 0;
		}
	}

	// [ code ]
	if ( !parseStatement(m_unitTop, ';', returnCalled, opcodeToReturn) )
	{
		return false;
	}

	// <- continue point
	setRelativeJumpTarget( m_units[m_unitTop].bytecode, *m_continueTargets.tail() );

	// [post code]
	appendBytecode( m_units[m_unitTop].bytecode, post.bytecode );

	addRelativeJumpSource( m_units[m_unitTop].bytecode, O_RelativeJump, conditionPoint );

	setRelativeJumpTarget( m_units[m_unitTop].bytecode, *m_breakTargets.tail() );

	m_continueTargets.pop();
	m_breakTargets.pop();

	resolveRelativeJumps( m_units[m_unitTop].bytecode );

	return true;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::parseIf( bool& returnCalled, WROpcode opcodeToReturn )
{
	WRExpressionContext ex;
	WRstr& token = ex.token;
	WRValue& value = ex.value;

	if ( !getToken(ex, "(") )
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}

	if ( !getToken(ex) )
	{
		m_err = WR_ERR_unexpected_EOF;
		return false;
	}

	WRExpression nex( m_units[m_unitTop].bytecode.localSpace );
	nex.context[0].token = token;
	nex.context[0].value = value;
	m_loadedToken = token;
	m_loadedValue = value;

	if ( parseExpression(nex) != ')' )
	{
		m_err = WR_ERR_unexpected_token;
		return false;
	}

	appendBytecode( m_units[m_unitTop].bytecode, nex.bytecode );

	int conditionFalseMarker = addRelativeJumpTarget( m_units[m_unitTop].bytecode );
	
	addRelativeJumpSource( m_units[m_unitTop].bytecode, O_BZ, conditionFalseMarker );

	if ( !parseStatement(m_unitTop, ';', returnCalled, opcodeToReturn) )
	{
		return false;
	}

 	if ( !getToken(ex) )
	{
		setRelativeJumpTarget(m_units[m_unitTop].bytecode, conditionFalseMarker);
	}
	else if ( token == "else" )
	{
		int conditionTrueMarker = addRelativeJumpTarget( m_units[m_unitTop].bytecode ); // when it hits here it will jump OVER this section

		addRelativeJumpSource( m_units[m_unitTop].bytecode, O_RelativeJump, conditionTrueMarker );
		
		setRelativeJumpTarget( m_units[m_unitTop].bytecode, conditionFalseMarker );

		if ( !parseStatement(m_unitTop, ';', returnCalled, opcodeToReturn) )
		{
			return false;
		}
		
		setRelativeJumpTarget( m_units[m_unitTop].bytecode, conditionTrueMarker );
	}
	else
	{
		m_loadedToken = token;
		m_loadedValue = value;
		setRelativeJumpTarget( m_units[m_unitTop].bytecode, conditionFalseMarker );
	}

	resolveRelativeJumps( m_units[m_unitTop].bytecode ); // at least do the ones we added

	return true;
}

//------------------------------------------------------------------------------
bool WRCompilationContext::parseStatement( int unitIndex, char end, bool& returnCalled, WROpcode opcodeToReturn )
{
	WRExpressionContext ex;
	returnCalled = false;

	for(;;)
	{
		WRstr& token = ex.token;

		if ( !getToken(ex) ) // if we run out of tokens that's fine as long as we were not waiting for a }
		{
			if ( end == '}' )
			{
				m_err = WR_ERR_unexpected_EOF;
				return false;
			}

			break;
		}

		if ( token[0] == end )
		{
			break;
		}

		if ( token == "{" )
		{
			return parseStatement( unitIndex, '}', returnCalled, opcodeToReturn );
		}

		if ( token == "return" )
		{
			returnCalled = true;
			if ( !getToken(ex) )
			{
				m_err = WR_ERR_unexpected_EOF;
				return false;
			}

			if ( token == ";" ) // special case of a null return, add the null
			{
				pushOpcode( m_units[unitIndex].bytecode, O_LiteralZero );
			}
			else
			{
				WRExpression nex( m_units[unitIndex].bytecode.localSpace );
				nex.context[0].token = token;
				nex.context[0].value = ex.value;
				m_loadedToken = token;
				m_loadedValue = ex.value;

				if ( parseExpression( nex ) != ';')
				{
					m_err = WR_ERR_unexpected_token;
					return false;
				}

				appendBytecode( m_units[unitIndex].bytecode, nex.bytecode );
			}

			pushOpcode( m_units[unitIndex].bytecode, opcodeToReturn );
		}
		else if ( token == "unit" || token == "function" || token == "struct" ) // ikr?
		{
			if ( !parseUnit() )
			{
				return false;
			}
		}
		else if ( token == "if" )
		{
			if ( !parseIf(returnCalled, opcodeToReturn) )
			{
				return false;
			}
		}
		else if ( token == "while" )
		{
			if ( !parseWhile(returnCalled, opcodeToReturn) )
			{
				return false;
			}
		}
		else if ( token == "for" )
		{
			if ( !parseForLoop(returnCalled, opcodeToReturn) )
			{
				return false;
			}
		}
		else if ( token == "do" )
		{
			if ( !parseDoWhile(returnCalled, opcodeToReturn) )
			{
				return false;
			}
		}
		else if ( token == "break" )
		{
			if ( !m_breakTargets.count() )
			{
				m_err = WR_ERR_break_keyword_not_in_looping_structure;
				return false;
			}

			addRelativeJumpSource( m_units[unitIndex].bytecode, O_RelativeJump, *m_breakTargets.tail() );
		}
		else if ( token == "continue" )
		{
			if ( !m_continueTargets.count() )
			{
				m_err = WR_ERR_continue_keyword_not_in_looping_structure;
				return false;
			}

			addRelativeJumpSource( m_units[unitIndex].bytecode, O_RelativeJump, *m_continueTargets.tail() );
		}
		else
		{
			WRExpression nex( m_units[unitIndex].bytecode.localSpace);
			nex.context[0].token = token;
			nex.context[0].value = ex.value;
			m_loadedToken = token;
			m_loadedValue = ex.value;
			if ( parseExpression(nex) != ';' )
			{
				m_err = WR_ERR_unexpected_token;
				return false;
			}

			appendBytecode( m_units[unitIndex].bytecode, nex.bytecode );
			pushOpcode( m_units[unitIndex].bytecode, O_PopOne );
		}

		if ( end == ';' ) // single statement
		{
			break;
		}
	}

	return true;
}

//------------------------------------------------------------------------------
void WRCompilationContext::createLocalHashMap( WRUnitContext& unit, unsigned char** buf, int* size )
{
	if ( unit.bytecode.localSpace.count() == 0 )
	{
		*size = 0;
		*buf = 0;
		return;
	}

	WRHashTable<unsigned char> offsets;
	for( unsigned char i=unit.arguments; i<unit.bytecode.localSpace.count(); ++i )
	{
		offsets.set( unit.bytecode.localSpace[i].hash, i - unit.arguments );
	}
	
	*size = 2;
	*buf = new unsigned char[ (offsets.m_mod * 5) + 4 ];
	(*buf)[(*size)++] = (offsets.m_mod>>8) & 0xFF;
	(*buf)[(*size)++] = offsets.m_mod & 0xFF;

	for( int i=0; i<offsets.m_mod; ++i )
	{
		(*buf)[(*size)++] = (offsets.m_list[i].hash>>24) & 0xFF;
		(*buf)[(*size)++] = (offsets.m_list[i].hash>>16) & 0xFF;
		(*buf)[(*size)++] = (offsets.m_list[i].hash>>8) & 0xFF;
		(*buf)[(*size)++] = offsets.m_list[i].hash & 0xFF;
		(*buf)[(*size)++] = offsets.m_list[i].hash ? offsets.m_list[i].value : (unsigned char)-1;
	}
}

//------------------------------------------------------------------------------
void WRCompilationContext::link( unsigned char** out, int* outLen )
{
	WROpcodeStream code;

	if ( m_units.count() > 1 )
	{
		code += O_FunctionListSize;
		code += (char)m_units.count() - 1;
	}

	unsigned char data[4];

	// register the function signatures
	for( unsigned int u=1; u<m_units.count(); ++u )
	{
		data[3] = u - 1; // index
		data[2] = m_units[u].arguments; // args
		data[1] = m_units[u].bytecode.localSpace.count(); // local frame size
		data[0] = 0;
		code += O_LiteralInt32;
		code.append( data, 4 ); // placeholder, it doesn't matter

		code += O_LiteralInt32; // hash
		code.append( pack32(m_units[u].hash, data), 4 );

		// offset placeholder
		code += O_LiteralInt32;
		m_units[u].offsetInBytecode = code.size();
		code.append( data, 4 ); // placeholder, it doesn't matter

		code += O_RegisterFunction;
	}

	// reserve global space
	if ( m_units[0].bytecode.localSpace.count() )
	{
		code += O_ReserveGlobalFrame;
		code += (unsigned char)(m_units[0].bytecode.localSpace.count());
	}

	// append all the unit code
	for( unsigned int u=0; u<m_units.count(); ++u )
	{
#ifdef _DUMP
		streamDump( m_units[u].bytecode.all );
#endif
		if ( u > 0 ) // for the non-zero unit fill location into the jump table
		{
			int32_t offset = code.size();
			pack32( offset, code.p_str(m_units[u].offsetInBytecode) );
		}

		int base = code.size();

		code.append( m_units[u].bytecode.all, m_units[u].bytecode.all.size() );

		// load new's
		for( unsigned int f=0; f<m_units[u].bytecode.unitObjectSpace.count(); ++f )
		{
			WRNamespaceLookup& N = m_units[u].bytecode.unitObjectSpace[f];

			for( unsigned int r=0; r<N.references.count(); ++r )
			{
				for( unsigned int u2 = 1; u2<m_units.count(); ++u2 )
				{
					if ( m_units[u2].hash != N.hash )
					{
						continue;
					}

					if ( m_units[u2].offsetOfLocalHashMap == 0 )
					{
						int size;
						unsigned char* buf = 0;

						createLocalHashMap( m_units[u2], &buf, &size );
						
						if ( size )
						{
							buf[0] = (unsigned char)(m_units[u2].bytecode.localSpace.count() - m_units[u2].arguments); // number of entries
							buf[1] = m_units[u2].arguments;
							m_units[u2].offsetOfLocalHashMap = code.size();
							code.append( buf, size );
						}

						delete[] buf;
					}

					if ( m_units[u2].offsetOfLocalHashMap != 0 )
					{
						int index = base + N.references[r];

						code[index] = (m_units[u2].offsetOfLocalHashMap>>8) & 0xFF;
						code[index+1] = m_units[u2].offsetOfLocalHashMap & 0xFF;
					}

					break;
				}
			}
		}
		
		// load function table
		for( unsigned int x=0; x<m_units[u].bytecode.functionSpace.count(); ++x )
		{
			WRNamespaceLookup& N = m_units[u].bytecode.functionSpace[x];

			for( unsigned int r=0; r<N.references.count(); ++r )
			{
				unsigned int u2 = 1;
				int index = base + N.references[r];

				for( ; u2<m_units.count(); ++u2 )
				{
					if ( m_units[u2].hash == N.hash )
					{
						code[index] = O_CallFunctionByIndex;

						code[index+2] = (char)(u2 - 1);

						// r+1 = args
						// r+2345 = hash
						// r+6

						if ( code[index+5] == O_PopOne || code[index + 6] == O_NewHashTable)
						{
							code[index+3] = 3; // skip past the pop, or TO the newHashTable
						}
						else 
						{
							code[index+3] = 2; // skip by this push is seen
							code[index+5] = O_PushIndexFunctionReturnValue;
						}
						
						break;
					}
				}

				if ( u2 >= m_units.count() )
				{
					if ( code[index+5] == O_PopOne )
					{
						code[index] = O_CallFunctionByHashAndPop;
					}
					else
					{
						code[index] = O_CallFunctionByHash;
					}

					pack32( N.hash, code.p_str(index+2) );
				}
			}
		}
	}


	if ( !m_err )
	{
		*outLen = code.size();
		code.release( out );
	}
}

//------------------------------------------------------------------------------
WRError WRCompilationContext::compile( const char* source,
									   const int size,
									   unsigned char** out,
									   int* outLen,
									   char* errorMsg )
{
	m_source = source;
	m_sourceLen = size;

	*outLen = 0;
	*out = 0;
	
	m_pos = 0;
	m_err = WR_ERR_None;
	m_EOF = false;
	m_unitTop = 0;

	m_units.setCount(1);
	
	bool returnCalled;

	m_loadedValue.p2 = INIT_AS_REF;

	do
	{
		parseStatement( 0, ';', returnCalled, O_Stop );

	} while ( !m_EOF && (m_err == WR_ERR_None) );

	if ( m_err != WR_ERR_None )
	{			
		int onChar = 0;
		int onLine = 1;
		WRstr line;
		WRstr msg;

		for( int p = 0; p<size && source[p] != '\n'; p++ )
		{
			line += (char)source[p];
		}

		for( int i=0; i<m_pos; ++i )
		{
			if ( source[i] == '\n' )
			{
				onLine++;
				onChar = 0;

				line.clear();
				for( int p = i+1; p<size && source[p] != '\n'; p++ )
				{
					line += (char)source[p];
				}
			}
			else
			{
				onChar++;
			}
		}

		msg.format( "line:%d\n", onLine );
		msg.appendFormat( "err:%d\n", m_err );
		msg.appendFormat( "%-5d %s\n", onLine, line.c_str() );

		for( int i=0; i<onChar; i++ )
		{
			msg.appendFormat(" ");
		}

		msg.appendFormat( "     ^\n" );

		if ( errorMsg )
		{
			strncpy( errorMsg, msg, msg.size() + 1 );
		}

		printf( "%s", msg.c_str() );
		
		return m_err;
	}
	
	if ( !returnCalled )
	{
		// pop final return value
		pushOpcode( m_units[0].bytecode, O_LiteralZero );
		pushOpcode( m_units[0].bytecode, O_Stop );
	}
	
	link( out, outLen );

	return m_err;
}

//------------------------------------------------------------------------------
int wr_compile( const char* source, const int size, unsigned char** out, int* outLen, char* errMsg )
{
	assert( sizeof(float) == 4 );
	assert( sizeof(int) == 4 );
	assert( sizeof(char) == 1 );
	assert( O_LAST < 255 );

	// create a compiler context that has all the necessary stuff so it's completely unloaded when complete
	WRCompilationContext comp; 

	return comp.compile( source, size, out, outLen, errMsg );
}

//------------------------------------------------------------------------------
#ifdef DEBUG_OPCODE_NAMES
const char* c_opcodeName[] = 
{
	"O_RegisterFunction",
	"O_ReserveGlobalFrame",
	"O_FunctionListSize",

	"O_LiteralInt32",
	"O_LiteralZero",
	"O_LiteralFloat",
	"O_LiteralString",

	"O_CallFunctionByHash",
	"O_CallFunctionByHashAndPop",
	"O_CallFunctionByIndex",
	"O_PushIndexFunctionReturnValue",

	"O_CallLibFunction",
	"O_CallLibFunctionAndPop",

	"O_NewHashTable",
	"O_AssignToHashTableByOffset",

	"O_PopOne",
	"O_Return",
	"O_Stop",

	"O_Index",
	"O_IndexSkipLoad",
	"O_CountOf",

	"O_StackIndexHash",
	"O_GlobalIndexHash",
	"O_LocalIndexHash",

	"O_StackSwap",
	"O_SwapTwoToTop",

	"O_LoadFromLocal",
	"O_LoadFromGlobal",

	"O_LLValues",
	"O_LGValues",
	"O_GLValues",
	"O_GGValues",

	"O_BinaryRightShiftSkipLoad",
	"O_BinaryLeftShiftSkipLoad",
	"O_BinaryAndSkipLoad",
	"O_BinaryOrSkipLoad",
	"O_BinaryXORSkipLoad",
	"O_BinaryModSkipLoad",

	"O_BinaryMultiplication",
	"O_BinarySubtraction",
	"O_BinaryDivision",
	"O_BinaryRightShift",
	"O_BinaryLeftShift",
	"O_BinaryMod",
	"O_BinaryOr",
	"O_BinaryXOR",
	"O_BinaryAnd",
	"O_BinaryAddition",

	"O_BitwiseNOT",

	"O_CoerceToInt",
	"O_CoerceToFloat",

	"O_RelativeJump",
	"O_RelativeJump8",

	"O_BZ",
	"O_BZ8",

	"O_LogicalAnd",
	"O_LogicalOr",
	"O_CompareLE",
	"O_CompareGE",
	"O_CompareGT",
	"O_CompareLT",
	"O_CompareEQ",
	"O_CompareNE",

	"O_GGCompareEQ", 
	"O_GGCompareNE", 
	"O_GGCompareGT",
	"O_GGCompareLT",
	
	"O_LLCompareGT",
	"O_LLCompareLT",
	"O_LLCompareEQ", 
	"O_LLCompareNE",

	"O_GSCompareEQ", 
	"O_LSCompareEQ", 
	"O_GSCompareNE", 
	"O_LSCompareNE", 
	"O_GSCompareGE",
	"O_LSCompareGE",
	"O_GSCompareLE",
	"O_LSCompareLE",
	"O_GSCompareGT",
	"O_LSCompareGT",
	"O_GSCompareLT",
	"O_LSCompareLT",

	"O_GSCompareEQBZ", 
	"O_LSCompareEQBZ", 
	"O_GSCompareNEBZ", 
	"O_LSCompareNEBZ", 
	"O_GSCompareGEBZ",
	"O_LSCompareGEBZ",
	"O_GSCompareLEBZ",
	"O_LSCompareLEBZ",
	"O_GSCompareGTBZ",
	"O_LSCompareGTBZ",
	"O_GSCompareLTBZ",
	"O_LSCompareLTBZ",

	"O_GSCompareEQBZ8",
	"O_LSCompareEQBZ8",
	"O_GSCompareNEBZ8",
	"O_LSCompareNEBZ8",
	"O_GSCompareGEBZ8",
	"O_LSCompareGEBZ8",
	"O_GSCompareLEBZ8",
	"O_LSCompareLEBZ8",
	"O_GSCompareGTBZ8",
	"O_LSCompareGTBZ8",
	"O_GSCompareLTBZ8",
	"O_LSCompareLTBZ8",

	"O_LLCompareLTBZ",
	"O_LLCompareGTBZ",
	"O_LLCompareEQBZ",
	"O_LLCompareNEBZ",
	"O_GGCompareLTBZ",
	"O_GGCompareGTBZ",
	"O_GGCompareEQBZ",
	"O_GGCompareNEBZ",

	"O_LLCompareLTBZ8",
	"O_LLCompareGTBZ8",
	"O_LLCompareEQBZ8",
	"O_LLCompareNEBZ8",
	"O_GGCompareLTBZ8",
	"O_GGCompareGTBZ8",
	"O_GGCompareEQBZ8",
	"O_GGCompareNEBZ8",

	"O_PostIncrement",
	"O_PostDecrement",
	"O_PreIncrement",
	"O_PreDecrement",

	"O_PreIncrementAndPop",
	"O_PreDecrementAndPop",

	"O_IncGlobal",
	"O_DecGlobal",
	"O_IncLocal",
	"O_DecLocal",

	"O_Assign",
	"O_AssignAndPop",
	"O_AssignToGlobalAndPop",
	"O_AssignToLocalAndPop",
	"O_AssignToArrayAndPop",

	"O_SubtractAssign",
	"O_AddAssign",
	"O_ModAssign",
	"O_MultiplyAssign",
	"O_DivideAssign",
	"O_ORAssign",
	"O_ANDAssign",
	"O_XORAssign",
	"O_RightShiftAssign",
	"O_LeftShiftAssign",

	"O_SubtractAssignAndPop",
	"O_AddAssignAndPop",
	"O_ModAssignAndPop",
	"O_MultiplyAssignAndPop",
	"O_DivideAssignAndPop",
	"O_ORAssignAndPop",
	"O_ANDAssignAndPop",
	"O_XORAssignAndPop",
	"O_RightShiftAssignAndPop",
	"O_LeftShiftAssignAndPop",

	"O_LogicalNot",
	"O_Negate",

	"O_LiteralInt8",
	"O_LiteralInt16",

	"O_IndexLiteral8",
	"O_IndexLiteral16",

	"O_IndexLocalLiteral8",
	"O_IndexGlobalLiteral8",
	"O_IndexLocalLiteral16",
	"O_IndexGlobalLiteral16",

	"O_BinaryAdditionAndStoreGlobal",
	"O_BinarySubtractionAndStoreGlobal",
	"O_BinaryMultiplicationAndStoreGlobal",
	"O_BinaryDivisionAndStoreGlobal",

	"O_BinaryAdditionAndStoreLocal",
	"O_BinarySubtractionAndStoreLocal",
	"O_BinaryMultiplicationAndStoreLocal",
	"O_BinaryDivisionAndStoreLocal",

	"O_CompareBEQ",
	"O_CompareBNE",
	"O_CompareBGE",
	"O_CompareBLE",
	"O_CompareBGT",
	"O_CompareBLT",

	"O_CompareBEQ8",
	"O_CompareBNE8",
	"O_CompareBGE8",
	"O_CompareBLE8",
	"O_CompareBGT8",
	"O_CompareBLT8",

	"O_BLA",
	"O_BLA8",
	"O_BLO",
	"O_BLO8",

	"O_LiteralInt8ToGlobal",
	"O_LiteralInt16ToGlobal",
	"O_LiteralInt32ToLocal",
	"O_LiteralInt8ToLocal",
	"O_LiteralInt16ToLocal",
	"O_LiteralFloatToGlobal",
	"O_LiteralFloatToLocal",
	"O_LiteralInt32ToGlobal",

	"O_GGBinaryMultiplication",
	"O_GLBinaryMultiplication",
	"O_LLBinaryMultiplication",

	"O_GGBinaryAddition",
	"O_GLBinaryAddition",
	"O_LLBinaryAddition",

	"O_GGBinarySubtraction",
	"O_GLBinarySubtraction",
	"O_LGBinarySubtraction",
	"O_LLBinarySubtraction",

	"O_GGBinaryDivision",
	"O_GLBinaryDivision",
	"O_LGBinaryDivision",
	"O_LLBinaryDivision",
};
#endif

#else // WRENCH_WITHOUT_COMPILER

int wr_compile( const char* source, const int size, unsigned char** out, int* outLen, char* errMsg )
{
	return WR_ERR_compiler_not_loaded;
}
	
#endif
