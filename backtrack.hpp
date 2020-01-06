#ifndef BACKTRACK_LIB_H
#define BACKTRACK_LIB_H
/* Author: Cole Blakley
   File: backtrack.hpp
   Purpose: The program is a sort of Prolog within C++. You create a Database
     with three type parameters: The type for the names of facts and
     rules (usually an enum class or string), and two other types, A and
     B, representing the two types of data that can be logically-related within
     facts/rules. To get started, instantiate a Database<> with
     these 3 types, in order, parameterized in the type.

     Facts are in form `Fact<A,B>(arg0, arg1, ..., argn)`, where arg0 through
     argn are of type A and/or type B. Facts represent a concrete association
     between 2 or more values. To add a Fact to a Database instance, use
     `db.add(factName, new Fact<A,B>(arg0, arg1, ..., argn))`.
*/
#include <map>
#include <vector>
#include <typeinfo>
#include <utility>
#include <algorithm>

// An object that can be potentially unified with another object
class Expression {
public:
    // `==` means "Can this expression unify with another?"
    virtual bool operator==(const Expression &o) const = 0;
    bool operator!=(const Expression &other) const
    {
	return !(*this == other);
    }
    // This operator means: try to unify any of these arguments
    // that you can; if you can't accept these args, return false
    virtual bool operator()(std::vector<Expression*> &args) = 0;
    // Has this Expression been unified yet?
    virtual bool is_unified() const = 0;
    // Change unification status of this Expression
    virtual void set_unified(bool val) = 0;
    virtual std::size_t arity() const = 0;
    virtual ~Expression() {}
};

// A placeholder representing an Atom<T> object with no value yet
template<typename T>
struct Variable {};

// A primitive value; wraps a C++ type and contains a single value
template<typename T>
class Atom : public Expression {
private:
    T m_value;
    bool m_is_unified;
public:
    explicit Atom() : m_is_unified(false) {}
    explicit Atom(T value) : m_value(value), m_is_unified(true) {}
    bool operator==(const Expression &o) const override
    {
	auto &other = (const Atom<T>&)o;
	return typeid(o) == typeid(Atom<T>&)
	    && (other.m_value == m_value
		|| other.m_is_unified != m_is_unified);
    }
    bool operator()(std::vector<Expression*> &args) override
    {
	auto &arg_ref{*args[0]};
	if(args.size() != 1 || typeid(arg_ref) != typeid(Atom<T>&))
	    return false;
	auto *arg = (Atom<T>*)args[0];
	if(!arg->m_is_unified) {
	    // If arg hasn't been unified, unify it with this object's value
	    arg->m_value = m_value;
	    arg->set_unified(true);
	    return true;
	} else {
	    return arg->m_value == m_value;
	}
    }
    bool is_unified() const override { return m_is_unified; }
    void set_unified(bool val) override { m_is_unified = val; }
    T value() const { return m_value; }
    std::size_t arity() const override { return 1; }
};


std::vector<std::size_t> ununified_positions(const std::vector<Expression*> &args)
{
    std::size_t i = 0;
    std::vector<std::size_t> positions;
    for(const auto *each : args) {
        if(!each->is_unified())
	    positions.push_back(i);
	++i;
    }
    return positions;
}

void restore_ununified(const std::vector<std::size_t> &positions,
		      std::vector<Expression*> &args)
{
    for(auto pos : positions) {
	args[pos]->set_unified(false);
    }
}

// An ordered grouping of Atoms that expresses a relation between them
class Fact : public Expression {
private:
    std::vector<Expression*> m_parts;
public:
    explicit Fact(std::vector<Expression*> parts)
	: m_parts(parts)
    {}

    bool operator==(const Expression &o) const override
    {
	if(typeid(o) != typeid(Fact))
	    return false;
	auto &other = (const Fact&)o;
	if(other.m_parts.size() != m_parts.size())
	    return false;

	for(std::size_t i = 0; i < m_parts.size(); ++i) {
	    if(*(m_parts[i]) != *(other.m_parts[i]))
		return false;
	}
	return true;
    }
    bool operator()(std::vector<Expression*> &args) override
    {
	if(args.size() != m_parts.size())
	    return false;

	std::vector<std::size_t> ununified = ununified_positions(args);
        for(std::size_t i = 0; i < m_parts.size(); ++i) {
	    std::vector<Expression*> arg{args[i]};
	    if(!(*m_parts[i])(arg)) {
		// Undo any unifications done by the Expressions in
		// m_parts
		restore_ununified(ununified, args);
		return false;
	    }
	}
	return true;
    }
    bool is_unified() const override { return true; }
    void set_unified(bool) override {}
    std::size_t arity() const override { return m_parts.size(); }
};

// A generalized Fact that can show relations between one or more Facts
// given arbitrary input arguments
class Rule : public Expression {
private:
    std::vector<Expression*> m_args;
    std::vector<Expression*> m_predicates;
public:
    explicit Rule(std::vector<Expression*> args) : m_args(args)
    {}
    bool operator==(const Expression &other) const override
    {
	return typeid(other) == typeid(Rule)
	    && ((const Rule&)other).m_args == m_args
	    && ((const Rule&)other).m_predicates == m_predicates;
    }
    bool operator()(std::vector<Expression*> &args) override
    {
	if(args.size() != m_args.size())
	    return false;

	return false;
    }
    Expression* operator[](std::size_t index)
    {
	return m_args[index];
    }
    /*Define the series of Expressions making up this Rule*/
    Rule& operator<<(Expression *part)
    {
	m_predicates.push_back((Expression*)part);
	return *this;
    }
    Rule& operator,(Expression *part)
    {
	return (*this << part);
    }
    bool is_unified() const override { return true; }
    void set_unified(bool) override {}
    std::size_t arity() const override { return m_args.size(); }
};

// Contains a group of named expressions; can be queried
template<typename T>
class Database {
private:
    std::map<T,std::vector<Expression*>> m_expressions;
    std::vector<Expression*> m_atoms;
    template<typename U>
    void add_atom(U atom, std::vector<Expression*> &so_far)
    {
	auto *new_atom = new Atom<U>(atom);
	// User enters a C++ value of type U
	m_atoms.push_back(new_atom);
	so_far.push_back(new_atom);
    }
    template<typename U>
    void add_atom(Variable<U>, std::vector<Expression*> &so_far)
    {
	auto *new_atom = new Atom<U>;
	m_atoms.push_back(new_atom);
	so_far.push_back(new_atom);
    }
    void add_arg(Expression *expr, std::vector<Expression*> &so_far)
    {
	so_far.push_back(expr);
    }
    template<typename U>
    void add_arg(Variable<U>, std::vector<Expression*> &so_far)
    {
	auto *new_atom = new Atom<U>;
	m_atoms.push_back(new_atom);
	so_far.push_back(new_atom);
    }
public:
    ~Database()
    {
	for(auto &[key, list] : m_expressions) {
	    for(auto *expr : list) {
		delete expr;
	    }
	}
	for(auto *atom : m_atoms) {
	    delete atom;
	}
    }
    template<typename ...Args>
    Fact& add_fact(T name, Args... atoms)
    {
	std::vector<Expression*> atoms_so_far;
	(add_atom(atoms, atoms_so_far), ...);

	auto *new_fact = new Fact(atoms_so_far);
	m_expressions[name].push_back(new_fact);
	return *new_fact;
    }
    template<typename ...Args>
    Rule& add_rule(T name, Args... args)
    {
	std::vector<Expression*> args_so_far;
	(add_arg(args, args_so_far), ...);
	auto *new_rule = new Rule(args_so_far);
	m_expressions[name].push_back(new_rule);
	return *new_rule;
    }
    Expression* get(T name, std::size_t arity)
    {
	if(m_expressions.find(name) == m_expressions.end())
	    return nullptr;
	auto match = std::find_if(m_expressions[name].begin(),
				  m_expressions[name].end(),
				  [arity](const auto *expr) {
				      return expr->arity() == arity;
				  });
	if(match == m_expressions[name].end())
	    return nullptr;
	else
	    return *match;
    }
    /*bool query(T name, Expression *question)
    {
	if(m_expressions.find(name) == m_expressions.end()) {
	    return false;
	}
	return true;
	}*/
};
#endif
