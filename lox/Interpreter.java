package lox;

import java.util.List;

import lox.Expr.Assign;
import lox.Expr.Binary;
import lox.Expr.Grouping;
import lox.Expr.Literal;
import lox.Expr.Unary;
import lox.Expr.Variable;
import lox.Stmt.Block;
import lox.Stmt.Expression;
import lox.Stmt.Print;
import lox.Stmt.Var;

public class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {

  // the current environment
  private Environment environment = new Environment();

  void interpret(List<Stmt> statements) {
    try {
      for (Stmt statement : statements) {
        execute(statement);
      }
    } catch (RuntimeError error) {
      Lox.runtimeError(error);
    }
  }

  @Override
  public Object visitBinaryExpr(Binary expr) {
    Object left = evaluate(expr.left);
    Object right = evaluate(expr.right);

    switch (expr.operator.type) {
      case GREATER:
        checkNumberOperands(expr.operator, left, right);
        return (double)left > (double) right;

      case GREATER_EQUAL:
        checkNumberOperands(expr.operator, left, right);
        return (double)left >= (double) right;

      case LESS:
        checkNumberOperands(expr.operator, left, right);
        return (double)left < (double) right;

      case LESS_EQUAL:
        checkNumberOperands(expr.operator, left, right);
        return (double)left <= (double) right;

      case BANG_EQUAL: return !isEqual(left, right);
      case EQUAL_EQUAL: return isEqual(left, right);

      case MINUS:
        checkNumberOperands(expr.operator, left, right);
        return (double)left - (double)right;
      
      case PLUS:
        if (
          left instanceof Double &&
          right instanceof Double
        ) {
          return (double)left + (double)right;
        }

        if (
          left instanceof String &&
          right instanceof String
        ) {
          return (String)left + (String)right;
        }

        throw new RuntimeError(expr.operator, "operands must be two numbers or two strings");
      
      case SLASH:
        checkNumberOperands(expr.operator, left, right);
        return (double)left / (double)right;
      
      case STAR:
        checkNumberOperands(expr.operator, left, right);
        return (double)left * (double)right;
      
      default:
        break;
    }

    // unreachable
    return null;
  }

  @Override
  public Object visitGroupingExpr(Grouping expr) {
    return evaluate(expr.expression);
  }

  @Override
  public Object visitLiteralExpr(Literal expr) {
    return expr.value;
  }

  @Override
  public Object visitUnaryExpr(Unary expr) {
    Object right = evaluate(expr.right);

    switch (expr.operator.type) {
      case BANG:
        return !isTruthy(right);
      case MINUS:
        checkNumberOperand(expr.operator, right);
        return -(double)right;
      
      default:
        break;
    }

    // unreachable?
    return null;
  }

  private void checkNumberOperand(
    Token operator,
    Object operand
  ) {
    if (operand instanceof Double) return;
    throw new RuntimeError(operator, "operand must be a number");
  }

  private void checkNumberOperands(
    Token operator,
    Object left,
    Object right
  ) {
    if (
      left instanceof Double &&
      right instanceof Double
    ) return;

    throw new RuntimeError(operator, "operand must be a number");
  }

  private boolean isTruthy(Object object) {
    if (object == null) return false;
    if (object instanceof Boolean) return (boolean)object;
    return true;
  }

  private boolean isEqual(
    Object a,
    Object b
  ) {
    if (a == null && b == null) return true;
    if (a == null) return false;

    return a.equals(b);
  }

  private String stringify(Object object) {
    if (object == null) return "nil";

    if (object instanceof Double) {
      String text = object.toString();

      if (text.endsWith(".0")) {
        text = text.substring(0, text.length() - 2);
      }

      return text;
    }

    return object.toString();
  }

  private Object evaluate(Expr expr) {
    return expr.accept(this);
  }

  private void execute(Stmt stmt) {
    stmt.accept(this);
  }

  private void executeBlock(
    List<Stmt> statements,
    Environment environment
  ) {
    Environment previous = this.environment;

    try {
      this.environment = environment;

      for (Stmt statement : statements) {
        execute(statement);
      }
    } finally {
      this.environment = previous;
    }
  }

  @Override
  public Void visitExpressionStmt(Expression stmt) {
    evaluate(stmt.expression);
    return null;
  }

  @Override
  public Void visitPrintStmt(Print stmt) {
    Object value = evaluate(stmt.expression);
    System.out.println(stringify(value));
    return null;
  }

  @Override
  public Void visitVarStmt(Var stmt) {
    Object value = null;

    if (stmt.initializer != null) {
      value = evaluate(stmt.initializer);
    }

    environment.define(stmt.name.lexeme, value);

    return null;
  }

  @Override
  public Object visitVariableExpr(Variable expr) {
    return environment.get(expr.name);
  }

  @Override
  public Object visitAssignExpr(Assign expr) {
    Object value = evaluate(expr.value);
    environment.assign(expr.name, value);
    return value;
  }

  @Override
  public Void visitBlockStmt(Block stmt) {
    executeBlock(stmt.statements, new Environment(environment));
    return null;
  }
}
