#include "lexer.h"
#include "parser.h"
#include "type.h"

#include <cassert>

ASTNode* parse_compound_statement(Lexer*, Scope*);
ASTNode* parse_jump_statement(Lexer*, Scope*);
ASTNode* parse_expression_statement(Lexer*, Scope*);
ASTNode* parse_iteration_statement(Lexer*, Scope*);
ASTNode* parse_selection_statement(Lexer*, Scope*);
ASTNode* parse_labeled_statement(Lexer*, Scope*);

static ExternalDeclaration* new_external_declaration(ExternalDeclarationType type, ASTNode const* head_node)
{
  ExternalDeclaration* new_ext_dec = (ExternalDeclaration*)malloc(sizeof(ExternalDeclaration));

  new_ext_dec->next = nullptr;
  new_ext_dec->root_ast_node = head_node;
  new_ext_dec->type = type;

  return new_ext_dec;
}

Type const*
declaration_to_fundamental_type(DeclarationSpecifierFlags* declaration)
{

  FundamentalType fundamental_type = fundamental_type_from_declaration(declaration);

  return get_fundamental_type_pointer(fundamental_type);
}

// 6.8 Statements
//      labeled statement
//      compound statement
//      expression statement
//      selection statement
//      iteration statement
//      jump statement
ASTNode* parse_statement(Lexer* lexer, Scope* scope)
{
  ASTNode* ast_node = new_ast_node(ASTNodeType::Void);
  switch (get_current_token(lexer)->type) {

  case TokenType::Identifier:
  case TokenType::Case:
  case TokenType::Default:
    return parse_labeled_statement(lexer, scope);

  case TokenType::LBrace:
    return parse_compound_statement(lexer, scope);

  case TokenType::If:
  case TokenType::Switch:
    return parse_selection_statement(lexer, scope);

  case TokenType::While:
  case TokenType::For:
  case TokenType::Do:
    return parse_iteration_statement(lexer, scope);

  case TokenType::GoTo:
  case TokenType::Continue:
  case TokenType::Break:
  case TokenType::Return:
    return parse_jump_statement(lexer, scope);

  default:
    return parse_expression_statement(lexer, scope);
  }

  return ast_node;
}

// labeled statements
//      identifier : statement for use with goto
//      case const-expression : statement
//      default : statement
ASTNode* parse_labeled_statement(Lexer* lexer, Scope* scope)
{
  (void)scope;
  ASTNode* ast_node = new_ast_node(ASTNodeType::Void);
  get_current_token(lexer);
  return ast_node;
}

// compound statement are blocks of declarations and other statements wrapped in
// {}, for use in basically everything, e.g. for loops
//
// compound-statement: ( declaration | statement )*
ASTNode* parse_compound_statement(Lexer* lexer, Scope* scope)
{
  assert(get_current_token(lexer)->type == TokenType::LBrace);
  get_next_token(lexer);

  ASTNode ast_node_anchor;
  ast_node_anchor.next = nullptr;
  ASTNode* previous_ast_node = &ast_node_anchor;

  while (get_current_token(lexer)->type != TokenType::RBrace) {

    ASTNode* current_ast_node;
    if (token_is_declaration_specifier(get_current_token(lexer), scope))
      current_ast_node = parse_declaration(lexer, scope);
    else
      current_ast_node = parse_statement(lexer, scope);

    previous_ast_node->next = current_ast_node;
    previous_ast_node = previous_ast_node->next;
  }

  assert(get_current_token(lexer)->type == TokenType::RBrace);
  get_next_token(lexer);
  return ast_node_anchor.next;
}

// expression statements are expr(opt);
ASTNode* parse_expression_statement(Lexer* lexer, Scope* scope)
{
  (void)scope;
  ASTNode* ast_node = new_ast_node(ASTNodeType::Void);
  get_current_token(lexer);
  return ast_node;
}

// selection statements are ifs/switches
ASTNode* parse_selection_statement(Lexer* lexer, Scope* scope)
{
  (void)scope;
  ASTNode* ast_node = new_ast_node(ASTNodeType::Void);
  get_current_token(lexer);
  return ast_node;
}

// iteration statements are (do) while and for
ASTNode* parse_iteration_statement(Lexer* lexer, Scope* scope)
{
  (void)scope;
  ASTNode* ast_node = new_ast_node(ASTNodeType::Void);
  get_current_token(lexer);
  return ast_node;
}

// jumps are goto identifier; continue; break; return;
ASTNode* parse_jump_statement(Lexer* lexer, Scope* scope)
{
  // FIXME: jump statement semantics
  ASTNode* ast_node = new_ast_node(ASTNodeType::Void);
  switch (get_current_token(lexer)->type) {
    get_next_token(lexer);

  case TokenType::GoTo: {
    Token const* identifier_token = get_current_token(lexer);
    if (identifier_token->type != TokenType::Identifier)
      error_token(lexer, "Expected identifier after goto\n");
  }

  case TokenType::Return: {
    get_next_token(lexer);
    if (get_current_token(lexer)->type != TokenType::Semicolon) {
      ASTNode* return_value_node = parse_expression(lexer, scope);
      ast_node->rhs = return_value_node;
    }
  }

  case TokenType::Continue:
  case TokenType::Break:
    expect_and_get_next_token(lexer, TokenType::Semicolon, "Expected semicolon after jump statement\n");
    break;

  default:
    assert(false);
    break;
  }

  return ast_node;
}

// a translation unit is ( function definition | declaration )*
//
// function-definition:
//      declaration-specifiers declarator declaration-list(opt)
//      compound-statement
// declaration
//      declaration-specifiers (declarator ( = initializer )?)*;
//
// both start with declaration specifiers and declarators
// if the declarator declares a function and is followed by a compound
// statement, we have a function definition
ExternalDeclaration* parse_translation_unit(char const* file)
{
  Lexer lexer = new_lexer(file);
  Scope current_scope;
  current_scope.parent_scope = nullptr;

  ExternalDeclaration declaration_anchor;
  declaration_anchor.next = nullptr;
  declaration_anchor.root_ast_node = nullptr;
  ExternalDeclaration* previous_declaration = &declaration_anchor;

  for (get_next_token(&lexer);
       get_current_token(&lexer)->type != TokenType::Eof;) {

    if (!token_is_declaration_specifier(get_current_token(&lexer),
            &current_scope))
      error_token(&lexer, "Expected declaration specifier\n");

    // parse declaration specifiers and turn to type
    DeclarationSpecifierFlags declaration_specifiers = parse_declaration_specifiers(&lexer, &current_scope);
    Type const* fundamental_type_ptr = declaration_to_fundamental_type(&declaration_specifiers);

    // prepare to parse declaration - overwrite declaration types if we find a function definition in the switch
    ASTNode* ast_node = new_ast_node(ASTNodeType::Declaration);
    ExternalDeclarationType declaration_type = ExternalDeclarationType::Declaration;

    ast_node->object = parse_declarator(&lexer, fundamental_type_ptr, &current_scope);

    switch (ast_node->object->type->fundamental_type) {
    case FundamentalType::Function:
      // if the current object is a function followed by a {, this is a function definition
      if (get_current_token(&lexer)->type == TokenType::LBrace) {
        declaration_type = ExternalDeclarationType::FunctionDefinition;
        ast_node->object->function_body = parse_compound_statement(&lexer, &current_scope);
        break;
      }

      // otherwise, whether a function or not, continue parsing a declaration
    default:
      parse_rest_of_declaration(&lexer, &current_scope, ast_node);
    }

    ExternalDeclaration* current_declaration = new_external_declaration(declaration_type, ast_node);
    previous_declaration->next = current_declaration;
    previous_declaration = current_declaration;
  } // end for loop

  return declaration_anchor.next;
}
