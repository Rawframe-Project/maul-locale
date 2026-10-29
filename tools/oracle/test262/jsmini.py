# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Sirac Ozmen
#
# A small evaluator for the subset of JavaScript test262's Temporal calendar
# tests use. It reads the tests as data: no file, process or network access
# exists in it, and every name it does not know raises Unsupported.

import math
import re


class Unsupported(Exception):
    pass


class JSThrow(Exception):
    def __init__(self, value):
        super().__init__(value)
        self.value = value


class Undefined:
    def __repr__(self):
        return "undefined"


class Null:
    def __repr__(self):
        return "null"


UNDEF = Undefined()
NULL = Null()

# ---------------------------------------------------------------- lexer

PUNCT = sorted("""=== !== ... => == != <= >= && || ?? ?. ++ -- += -= *= /= ** { } ( ) [ ] ; , < > + - * / % ! = ? : . ~ & | ^""".split(), key=len, reverse=True)
KEYWORDS = {"var", "let", "const", "function", "return", "if", "else", "for", "of", "in", "while",
            "new", "typeof", "true", "false", "null", "undefined", "break", "continue", "throw", "class",
            "do", "switch", "case", "default", "try", "catch", "finally", "this", "void", "delete",
            "instanceof"}


def lex(src):
    toks = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c in " \t\r\n":
            i += 1
        elif src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            if j < 0:
                raise Unsupported("unterminated comment")
            i = j + 2
        elif c.isdigit() or (c == "." and i + 1 < n and src[i + 1].isdigit()):
            m = re.compile(r"0[xX][0-9a-fA-F_]+n?|(\d[\d_]*)?(\.\d[\d_]*)?([eE][+-]?\d+)?n?").match(src, i)
            t = m.group(0).replace("_", "")
            if t.endswith("n"):
                if "." in t or "e" in t.lower()[2 if t[:2] in ("0x", "0X") else 0:-1]:
                    raise Unsupported("bigint literal " + t)
                toks.append(("num", BigInt(int(t[:-1], 16 if t[:2] in ("0x", "0X") else 10))))
                i = m.end()
                continue
            v = int(t, 16) if t[:2] in ("0x", "0X") else float(t)
            if isinstance(v, float) and v.is_integer() and abs(v) < 2**53:
                v = int(v)
            toks.append(("num", v))
            i = m.end()
        elif c.isalpha() or c in "_$":
            m = re.compile(r"(?:[^\W\d]|\$)[\w$]*").match(src, i)
            w = m.group(0)
            toks.append(("kw" if w in KEYWORDS else "id", w))
            i = m.end()
        elif c in "\"'":
            j = i + 1
            out = []
            while src[j] != c:
                if src[j] == "\\":
                    out.append(unescape(src, j))
                    j += escape_len(src, j)
                else:
                    out.append(src[j])
                    j += 1
            toks.append(("str", "".join(out)))
            i = j + 1
        elif c == "`":
            j = i + 1
            parts = []  # alternating str, token list
            cur = []
            while src[j] != "`":
                if src[j] == "\\":
                    cur.append(unescape(src, j))
                    j += escape_len(src, j)
                elif src.startswith("${", j):
                    depth = 1
                    k = j + 2
                    while depth:
                        if src[k] == "{":
                            depth += 1
                        elif src[k] == "}":
                            depth -= 1
                        k += 1
                    parts.append("".join(cur))
                    cur = []
                    parts.append(lex(src[j + 2:k - 1]))
                    j = k
                else:
                    cur.append(src[j])
                    j += 1
            parts.append("".join(cur))
            toks.append(("tpl", parts))
            i = j + 1
        else:
            for p in PUNCT:
                if src.startswith(p, i):
                    toks.append(("p", p))
                    i += len(p)
                    break
            else:
                raise Unsupported("character %r" % c)
    toks.append(("eof", None))
    return toks


def escape_len(src, j):
    c = src[j + 1]
    if c == "u":
        if src[j + 2] == "{":
            return src.index("}", j) - j + 1
        return 6
    if c == "x":
        return 4
    return 2


def unescape(src, j):
    c = src[j + 1]
    if c == "u":
        if src[j + 2] == "{":
            return chr(int(src[j + 3:src.index("}", j)], 16))
        return chr(int(src[j + 2:j + 6], 16))
    if c == "x":
        return chr(int(src[j + 2:j + 4], 16))
    return {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "b": "\b", "f": "\f", "v": "\v"}.get(c, c)

# ---------------------------------------------------------------- parser


class Parser:
    def __init__(self, toks):
        self.t = toks
        self.i = 0

    def peek(self, k=0):
        return self.t[self.i + k]

    def next(self):
        tok = self.t[self.i]
        self.i += 1
        return tok

    def at(self, kind, val=None):
        tok = self.t[self.i]
        return tok[0] == kind and (val is None or tok[1] == val)

    def atp(self, val):
        return self.at("p", val)

    def eat(self, kind, val=None):
        if self.at(kind, val):
            return self.next()
        return None

    def expect(self, kind, val=None):
        tok = self.next()
        if tok[0] != kind or (val is not None and tok[1] != val):
            raise Unsupported("expected %s %s, got %r" % (kind, val, tok))
        return tok

    def program(self):
        body = []
        while not self.at("eof"):
            body.append(self.statement())
        return body

    def statement(self):
        tok = self.peek()
        if tok == ("p", "{"):
            return self.block()
        if tok == ("p", ";"):
            self.next()
            return ("empty",)
        if tok[0] == "kw":
            k = tok[1]
            if k in ("var", "let", "const"):
                s = self.declaration()
                self.eat("p", ";")
                return s
            if k == "function":
                self.next()
                name = self.expect("id")[1]
                params, body = self.function_rest()
                return ("fundecl", name, params, body)
            if k == "return":
                self.next()
                if self.atp(";") or self.atp("}"):
                    self.eat("p", ";")
                    return ("return", None)
                e = self.expression()
                self.eat("p", ";")
                return ("return", e)
            if k == "if":
                self.next()
                self.expect("p", "(")
                c = self.expression()
                self.expect("p", ")")
                a = self.statement()
                b = None
                if self.eat("kw", "else"):
                    b = self.statement()
                return ("if", c, a, b)
            if k == "while":
                self.next()
                self.expect("p", "(")
                c = self.expression()
                self.expect("p", ")")
                return ("while", c, self.statement())
            if k == "for":
                return self.for_statement()
            if k in ("break", "continue"):
                self.next()
                self.eat("p", ";")
                return (k,)
            if k == "throw":
                self.next()
                e = self.expression()
                self.eat("p", ";")
                return ("throw", e)
            if k == "try":
                self.next()
                body = self.block()
                param = None
                handler = None
                fin = None
                if self.eat("kw", "catch"):
                    if self.eat("p", "("):
                        param = self.expect("id")[1]
                        self.expect("p", ")")
                    handler = self.block()
                if self.eat("kw", "finally"):
                    fin = self.block()
                return ("try", body, param, handler, fin)
            if k in ("class", "switch", "do"):
                raise Unsupported(k)
        e = self.expression()
        self.eat("p", ";")
        return ("expr", e)

    def block(self):
        self.expect("p", "{")
        body = []
        while not self.atp("}"):
            body.append(self.statement())
        self.next()
        return ("block", body)

    def declaration(self):
        kind = self.next()[1]
        decls = []
        while True:
            target = self.pattern()
            init = None
            if self.eat("p", "="):
                init = self.assignment()
            decls.append((target, init))
            if not self.eat("p", ","):
                break
        return ("decl", kind, decls)

    def pattern(self):
        if self.eat("p", "["):
            items = []
            while not self.atp("]"):
                if self.atp(","):
                    self.next()
                    items.append(None)
                    continue
                if self.eat("p", "..."):
                    items.append(("rest", self.pattern()))
                else:
                    p = self.pattern()
                    if self.eat("p", "="):
                        p = ("default", p, self.assignment())
                    items.append(p)
                if not self.eat("p", ","):
                    break
            self.expect("p", "]")
            return ("arraypat", items)
        if self.eat("p", "{"):
            props = []
            while not self.atp("}"):
                key = self.next()
                if key[0] not in ("id", "kw", "str"):
                    raise Unsupported("object pattern key")
                name = key[1]
                if self.eat("p", ":"):
                    p = self.pattern()
                else:
                    p = ("name", name)
                if self.eat("p", "="):
                    p = ("default", p, self.assignment())
                props.append((name, p))
                if not self.eat("p", ","):
                    break
            self.expect("p", "}")
            return ("objpat", props)
        return ("name", self.expect("id")[1])

    def for_statement(self):
        self.next()
        self.expect("p", "(")
        save = self.i
        if self.peek()[0] == "kw" and self.peek()[1] in ("var", "let", "const"):
            kind = self.next()[1]
            target = self.pattern()
            if self.eat("kw", "of"):
                it = self.assignment()
                self.expect("p", ")")
                return ("forof", kind, target, it, self.statement())
            if self.eat("kw", "in"):
                it = self.expression()
                self.expect("p", ")")
                return ("forin", kind, target, it, self.statement())
            self.i = save
        init = None
        if not self.atp(";"):
            if self.peek()[0] == "kw" and self.peek()[1] in ("var", "let", "const"):
                init = self.declaration()
            else:
                init = ("expr", self.expression())
        self.expect("p", ";")
        cond = None if self.atp(";") else self.expression()
        self.expect("p", ";")
        upd = None if self.atp(")") else self.expression()
        self.expect("p", ")")
        return ("for", init, cond, upd, self.statement())

    def function_rest(self):
        self.expect("p", "(")
        params = []
        while not self.atp(")"):
            if self.eat("p", "..."):
                params.append(("rest", self.pattern()))
            else:
                p = self.pattern()
                if self.eat("p", "="):
                    p = ("default", p, self.assignment())
                params.append(p)
            if not self.eat("p", ","):
                break
        self.expect("p", ")")
        return params, self.block()

    def expression(self):
        e = self.assignment()
        if self.atp(","):
            items = [e]
            while self.eat("p", ","):
                items.append(self.assignment())
            return ("seq", items)
        return e

    def is_arrow(self):
        if self.at("id") and self.peek(1) == ("p", "=>"):
            return True
        if not self.atp("("):
            return False
        depth = 0
        j = self.i
        while True:
            tok = self.t[j]
            if tok == ("p", "(") or tok == ("p", "[") or tok == ("p", "{"):
                depth += 1
            elif tok == ("p", ")") or tok == ("p", "]") or tok == ("p", "}"):
                depth -= 1
                if depth == 0:
                    return self.t[j + 1] == ("p", "=>")
            elif tok[0] == "eof":
                return False
            j += 1

    def assignment(self):
        if self.is_arrow():
            if self.at("id"):
                params = [("name", self.next()[1])]
            else:
                self.expect("p", "(")
                params = []
                while not self.atp(")"):
                    if self.eat("p", "..."):
                        params.append(("rest", self.pattern()))
                    else:
                        p = self.pattern()
                        if self.eat("p", "="):
                            p = ("default", p, self.assignment())
                        params.append(p)
                    if not self.eat("p", ","):
                        break
                self.expect("p", ")")
            self.expect("p", "=>")
            if self.atp("{"):
                body = self.block()
            else:
                body = ("block", [("return", self.assignment())])
            return ("func", None, params, body)
        left = self.conditional()
        if self.peek()[0] == "p" and self.peek()[1] in ("=", "+=", "-=", "*=", "/="):
            op = self.next()[1]
            right = self.assignment()
            return ("assign", op, left, right)
        return left

    def conditional(self):
        c = self.binary(0)
        if self.eat("p", "?"):
            a = self.assignment()
            self.expect("p", ":")
            b = self.assignment()
            return ("cond", c, a, b)
        return c

    LEVELS = [["??"], ["||"], ["&&"], ["|"], ["^"], ["&"], ["===", "!==", "==", "!="],
              ["<", ">", "<=", ">=", "instanceof", "in"], ["+", "-"], ["*", "/", "%"], ["**"]]

    def binary(self, level):
        if level == len(self.LEVELS):
            return self.unary()
        left = self.binary(level + 1)
        while True:
            tok = self.peek()
            if tok[0] in ("p", "kw") and tok[1] in self.LEVELS[level]:
                self.next()
                right = self.binary(level + 1)
                left = ("bin", tok[1], left, right)
            else:
                return left

    def unary(self):
        tok = self.peek()
        if tok[0] == "p" and tok[1] in ("!", "-", "+", "~"):
            self.next()
            return ("unary", tok[1], self.unary())
        if tok[0] == "p" and tok[1] in ("++", "--"):
            self.next()
            return ("update", tok[1], True, self.unary())
        if tok[0] == "kw" and tok[1] in ("typeof", "void", "delete"):
            self.next()
            return ("unary", tok[1], self.unary())
        e = self.postfix()
        if self.peek()[0] == "p" and self.peek()[1] in ("++", "--"):
            op = self.next()[1]
            return ("update", op, False, e)
        return e

    def postfix(self):
        if self.eat("kw", "new"):
            callee = self.primary()
            while self.atp("."):
                self.next()
                callee = ("member", callee, ("lit", self.next()[1]))
            args = self.arguments() if self.atp("(") else []
            e = ("new", callee, args)
        else:
            e = self.primary()
        while True:
            if self.eat("p", "."):
                e = ("member", e, ("lit", self.next()[1]))
            elif self.eat("p", "?."):
                if self.atp("("):
                    raise Unsupported("optional call")
                e = ("member", e, ("lit", self.next()[1]))
            elif self.eat("p", "["):
                k = self.expression()
                self.expect("p", "]")
                e = ("member", e, k)
            elif self.atp("("):
                e = ("call", e, self.arguments())
            elif self.at("tpl"):
                raise Unsupported("tagged template")
            else:
                return e

    def arguments(self):
        self.expect("p", "(")
        args = []
        while not self.atp(")"):
            if self.eat("p", "..."):
                args.append(("spread", self.assignment()))
            else:
                args.append(self.assignment())
            if not self.eat("p", ","):
                break
        self.expect("p", ")")
        return args

    def primary(self):
        tok = self.next()
        k, v = tok
        if k == "num" or k == "str":
            return ("lit", v)
        if k == "tpl":
            return ("tpl", [p if isinstance(p, str) else Parser(p).expression() for p in v])
        if k == "id":
            return ("name", v)
        if k == "kw":
            if v == "true":
                return ("lit", True)
            if v == "false":
                return ("lit", False)
            if v == "null":
                return ("lit", NULL)
            if v == "undefined":
                return ("lit", UNDEF)
            if v == "function":
                name = self.next()[1] if self.at("id") else None
                params, body = self.function_rest()
                return ("func", name, params, body)
            if v == "this":
                raise Unsupported("this")
        if tok == ("p", "("):
            e = self.expression()
            self.expect("p", ")")
            return e
        if tok == ("p", "["):
            items = []
            while not self.atp("]"):
                if self.atp(","):
                    self.next()
                    items.append(("lit", UNDEF))
                    continue
                if self.eat("p", "..."):
                    items.append(("spread", self.assignment()))
                else:
                    items.append(self.assignment())
                if not self.eat("p", ","):
                    break
            self.expect("p", "]")
            return ("array", items)
        if tok == ("p", "{"):
            props = []
            while not self.atp("}"):
                if self.eat("p", "..."):
                    props.append(("spread", self.assignment()))
                elif self.eat("p", "["):
                    key = self.assignment()
                    self.expect("p", "]")
                    self.expect("p", ":")
                    props.append(("computed", key, self.assignment()))
                else:
                    key = self.next()
                    if key[0] == "num":
                        name = jsstr(key[1])
                    elif key[0] in ("id", "kw", "str"):
                        name = key[1]
                    else:
                        raise Unsupported("object key %r" % (key,))
                    if self.atp("("):
                        params, body = self.function_rest()
                        props.append(("prop", name, ("func", name, params, body)))
                    elif self.eat("p", ":"):
                        props.append(("prop", name, self.assignment()))
                    else:
                        if key[0] != "id":
                            raise Unsupported("shorthand")
                        props.append(("prop", name, ("name", name)))
                if not self.eat("p", ","):
                    break
            self.expect("p", "}")
            return ("object", props)
        raise Unsupported("token %r" % (tok,))

# ---------------------------------------------------------------- values


class JSObject:
    def __init__(self, props=None):
        self.props = dict(props or {})

    def get(self, key):
        return self.props.get(key, UNDEF)

    def set(self, key, value):
        self.props[key] = value


class JSArray:
    def __init__(self, items):
        self.items = list(items)


class JSFunction:
    def __init__(self, name, params, body, env, interp):
        self.name = name
        self.params = params
        self.body = body
        self.env = env
        self.interp = interp

    def call(self, args):
        return self.interp.call_function(self, args)


class Builtin:
    def __init__(self, fn, name=""):
        self.fn = fn
        self.name = name

    def call(self, args):
        return self.fn(*args)


class ErrorClass:
    def __init__(self, name):
        self.name = name


class Tracked:
    # A value read from a date, remembered with where it came from so an
    # assertion on it can be recorded.
    def __init__(self, value, origin, prop):
        self.value = value
        self.origin = origin
        self.prop = prop


def un(v):
    return v.value if isinstance(v, Tracked) else v


def jsstr(v):
    v = un(v)
    if v is UNDEF:
        return "undefined"
    if v is NULL:
        return "null"
    if v is True:
        return "true"
    if v is False:
        return "false"
    if isinstance(v, float):
        if math.isnan(v):
            return "NaN"
        if math.isinf(v):
            return "Infinity" if v > 0 else "-Infinity"
        if v.is_integer():
            return str(int(v))
        return repr(v)
    if isinstance(v, int):
        return str(v)
    if isinstance(v, BigInt):
        return str(v.v)
    if isinstance(v, str):
        return v
    if isinstance(v, JSArray):
        return ",".join("" if un(x) in (UNDEF, NULL) else jsstr(x) for x in v.items)
    if hasattr(v, "js_string"):
        return v.js_string()
    if isinstance(v, JSObject):
        return "[object Object]"
    raise Unsupported("string of %r" % (v,))


def truthy(v):
    v = un(v)
    if v is UNDEF or v is NULL or v is False:
        return False
    if v is True:
        return True
    if isinstance(v, (int, float)):
        return v != 0 and not (isinstance(v, float) and math.isnan(v))
    if isinstance(v, str):
        return v != ""
    if isinstance(v, BigInt):
        return v.v != 0
    return True


class BigInt:
    # A JavaScript BigInt: an exact integer of its own type, never mixed
    # with Numbers in arithmetic.
    __slots__ = ("v",)

    def __init__(self, v):
        self.v = int(v)

    def __eq__(self, other):
        return isinstance(other, BigInt) and other.v == self.v

    def __hash__(self):
        return hash(("bigint", self.v))

    def __repr__(self):
        return "%dn" % self.v


def bigint_op(op, a, b):
    if not isinstance(a, BigInt) or not isinstance(b, BigInt):
        if op in ("<", ">", "<=", ">=") and isinstance(a, (BigInt, int, float)) and isinstance(b, (BigInt, int, float)):
            x = a.v if isinstance(a, BigInt) else a
            y = b.v if isinstance(b, BigInt) else b
            return {"<": x < y, ">": x > y, "<=": x <= y, ">=": x >= y}[op]
        raise Unsupported("BigInt mixed with another type")
    x, y = a.v, b.v
    if op in ("<", ">", "<=", ">="):
        return {"<": x < y, ">": x > y, "<=": x <= y, ">=": x >= y}[op]
    if op == "+":
        return BigInt(x + y)
    if op == "-":
        return BigInt(x - y)
    if op == "*":
        return BigInt(x * y)
    if op in ("/", "%"):
        if y == 0:
            raise Unsupported("BigInt division by zero")
        q = abs(x) // abs(y) * (1 if (x < 0) == (y < 0) else -1)
        return BigInt(q if op == "/" else x - q * y)
    if op == "**":
        if y < 0:
            raise Unsupported("BigInt negative exponent")
        return BigInt(x ** y)
    raise Unsupported("BigInt operator " + op)


def tonum(v):
    v = un(v)
    if isinstance(v, BigInt):
        raise Unsupported("BigInt to Number")
    if isinstance(v, bool):
        return int(v)
    if isinstance(v, (int, float)):
        return v
    if v is UNDEF:
        return math.nan
    if v is NULL:
        return 0
    if isinstance(v, str):
        s = v.strip()
        if s == "":
            return 0
        try:
            f = float(s)
        except ValueError:
            return math.nan
        return int(f) if f.is_integer() and abs(f) < 2**53 else f
    raise Unsupported("number of %r" % (v,))


def norm(x):
    # A Number: an integer below 2^53 as an int, else the float64 nearest
    # (an exact int result past 2^53 rounds as JavaScript's arithmetic).
    if isinstance(x, float) and x.is_integer() and abs(x) < 2**53:
        return int(x)
    if isinstance(x, int) and not isinstance(x, bool) and abs(x) >= 2**53:
        return float(x)
    return x


def strict_equal(a, b):
    a, b = un(a), un(b)
    if isinstance(a, BigInt) or isinstance(b, BigInt):
        return a == b
    if isinstance(a, bool) or isinstance(b, bool):
        return a is b
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return a == b
    if isinstance(a, str) and isinstance(b, str):
        return a == b
    return a is b


def same_value(a, b):
    a, b = un(a), un(b)
    if isinstance(a, float) and isinstance(b, float) and math.isnan(a) and math.isnan(b):
        return True
    if isinstance(a, (int, float)) and not isinstance(a, bool) and isinstance(b, (int, float)) and not isinstance(b, bool):
        if a == 0 and b == 0:
            return math.copysign(1, a) == math.copysign(1, b)
    return strict_equal(a, b)


class Env:
    def __init__(self, parent=None):
        self.vars = {}
        self.parent = parent

    def lookup(self, name):
        e = self
        while e is not None:
            if name in e.vars:
                return e
            e = e.parent
        return None

    def get(self, name):
        e = self.lookup(name)
        if e is None:
            raise Unsupported("name " + name)
        return e.vars[name]

    def set(self, name, value):
        e = self.lookup(name)
        if e is None:
            raise Unsupported("assign to undeclared " + name)
        e.vars[name] = value

    def declare(self, name, value):
        self.vars[name] = value


class Return(Exception):
    def __init__(self, value):
        super().__init__()
        self.value = value


class Break(Exception):
    pass


class Continue(Exception):
    pass

# ---------------------------------------------------------------- interpreter


class Interp:
    def __init__(self, globals_):
        self.genv = Env()
        for k, v in globals_.items():
            self.genv.declare(k, v)
        self.steps = 0

    def run(self, src):
        prog = Parser(lex(src)).program()
        self.hoist(prog, self.genv)
        for s in prog:
            self.exec(s, self.genv)

    def hoist(self, body, env):
        for s in body:
            if s[0] == "fundecl":
                env.declare(s[1], JSFunction(s[1], s[2], s[3], env, self))
            elif s[0] == "decl" and s[1] == "var":
                for target, _ in s[2]:
                    for n in pattern_names(target):
                        if env.lookup(n) is None:
                            env.declare(n, UNDEF)

    def tick(self):
        self.steps += 1
        if self.steps > 20_000_000:
            raise Unsupported("step limit")

    def exec(self, s, env):
        self.tick()
        k = s[0]
        if k == "expr":
            self.eval(s[1], env)
        elif k == "decl":
            for target, init in s[2]:
                v = UNDEF if init is None else self.eval(init, env)
                if s[1] == "var":
                    self.bind(target, v, env, assign_var=True)
                else:
                    self.bind(target, v, env)
        elif k == "fundecl":
            pass
        elif k == "block":
            inner = Env(env)
            self.hoist(s[1], inner)
            for t in s[1]:
                self.exec(t, inner)
        elif k == "if":
            if truthy(self.eval(s[1], env)):
                self.exec(s[2], env)
            elif s[3] is not None:
                self.exec(s[3], env)
        elif k == "while":
            while truthy(self.eval(s[1], env)):
                try:
                    self.exec(s[2], env)
                except Break:
                    break
                except Continue:
                    continue
        elif k == "for":
            loop = Env(env)
            if s[1] is not None:
                self.exec(s[1], loop)
            while s[2] is None or truthy(self.eval(s[2], loop)):
                body = Env(loop)
                try:
                    self.exec(s[4], body)
                except Break:
                    break
                except Continue:
                    pass
                if s[3] is not None:
                    self.eval(s[3], loop)
        elif k == "forof":
            it = un(self.eval(s[3], env))
            items = self.iterate(it)
            for v in items:
                body = Env(env)
                self.bind(s[2], v, body, assign_var=(s[1] == "var"))
                try:
                    self.exec(s[4], body)
                except Break:
                    break
                except Continue:
                    pass
        elif k == "forin":
            obj = un(self.eval(s[3], env))
            if not isinstance(obj, JSObject):
                raise Unsupported("for in")
            for key in list(obj.props):
                body = Env(env)
                self.bind(s[2], key, body, assign_var=(s[1] == "var"))
                try:
                    self.exec(s[4], body)
                except Break:
                    break
                except Continue:
                    pass
        elif k == "return":
            raise Return(UNDEF if s[1] is None else self.eval(s[1], env))
        elif k == "break":
            raise Break()
        elif k == "continue":
            raise Continue()
        elif k == "throw":
            raise JSThrow(self.eval(s[1], env))
        elif k == "try":
            try:
                self.exec(s[1], env)
            except JSThrow as t:
                if s[3] is None:
                    raise
                inner = Env(env)
                if s[2]:
                    inner.declare(s[2], t.value)
                self.exec(s[3], inner)
            finally:
                if s[4] is not None:
                    self.exec(s[4], env)
        elif k == "empty":
            pass
        else:
            raise Unsupported("statement " + k)

    def iterate(self, it):
        if isinstance(it, JSArray):
            return list(it.items)
        if isinstance(it, str):
            return list(it)
        if isinstance(it, JSSet):
            return list(it.items)
        raise Unsupported("iterate %r" % (it,))

    def bind(self, target, v, env, assign_var=False):
        k = target[0]
        if k == "name":
            if assign_var:
                e = env.lookup(target[1])
                (e or env).vars[target[1]] = v
            else:
                env.declare(target[1], v)
        elif k == "default":
            if un(v) is UNDEF:
                v = self.eval(target[2], env)
            self.bind(target[1], v, env, assign_var)
        elif k == "arraypat":
            items = self.iterate(un(v))
            for i, p in enumerate(target[1]):
                if p is None:
                    continue
                if p[0] == "rest":
                    self.bind(p[1], JSArray(items[i:]), env, assign_var)
                    break
                self.bind(p, items[i] if i < len(items) else UNDEF, env, assign_var)
        elif k == "objpat":
            for name, p in target[1]:
                self.bind(p, self.get_member(un(v), name), env, assign_var)
        else:
            raise Unsupported("pattern " + k)

    def call_function(self, fn, args):
        env = Env(fn.env)
        for i, p in enumerate(fn.params):
            if p[0] == "rest":
                self.bind(p[1], JSArray(args[i:]), env)
                break
            self.bind(p, args[i] if i < len(args) else UNDEF, env)
        self.hoist(fn.body[1], env)
        try:
            for s in fn.body[1]:
                self.exec(s, env)
        except Return as r:
            return r.value
        return UNDEF

    def call(self, fn, args):
        fn = un(fn)
        if isinstance(fn, (JSFunction, Builtin)) or hasattr(fn, "call"):
            return fn.call(args)
        if isinstance(fn, ErrorClass):
            return make_error(fn, args[0] if args else "")
        raise Unsupported("call of %r" % (fn,))

    def eval_args(self, args, env):
        out = []
        for a in args:
            if a[0] == "spread":
                out.extend(self.iterate(un(self.eval(a[1], env))))
            else:
                out.append(self.eval(a, env))
        return out

    def eval(self, e, env):
        self.tick()
        k = e[0]
        if k == "lit":
            return e[1]
        if k == "name":
            return env.get(e[1])
        if k == "tpl":
            return "".join(p if isinstance(p, str) else jsstr(self.eval(p, env)) for p in e[1])
        if k == "array":
            out = []
            for a in e[1]:
                if a[0] == "spread":
                    out.extend(self.iterate(un(self.eval(a[1], env))))
                else:
                    out.append(self.eval(a, env))
            return JSArray(out)
        if k == "object":
            obj = JSObject()
            for p in e[1]:
                if p[0] == "prop":
                    obj.set(p[1], self.eval(p[2], env))
                elif p[0] == "computed":
                    obj.set(jsstr(self.eval(p[1], env)), self.eval(p[2], env))
                else:
                    src = un(self.eval(p[1], env))
                    if isinstance(src, JSObject):
                        obj.props.update(src.props)
                    elif src not in (UNDEF, NULL):
                        raise Unsupported("spread of %r" % (src,))
            return obj
        if k == "func":
            return JSFunction(e[1], e[2], e[3], env, self)
        if k == "member":
            obj = self.eval(e[1], env)
            key = self.eval(e[2], env)
            return self.get_member(obj, key)
        if k == "call":
            callee = e[1]
            if callee[0] == "member":
                obj = self.eval(callee[1], env)
                key = self.eval(callee[2], env)
                fn = self.get_member(obj, key)
            else:
                fn = self.eval(callee, env)
            return self.call(fn, self.eval_args(e[2], env))
        if k == "new":
            cls = un(self.eval(e[1], env))
            args = self.eval_args(e[2], env)
            if isinstance(cls, ErrorClass):
                return make_error(cls, args[0] if args else "")
            if hasattr(cls, "construct"):
                return cls.construct(args)
            raise Unsupported("new %r" % (cls,))
        if k == "assign":
            op, left, right = e[1], e[2], e[3]
            v = self.eval(right, env)
            if op != "=":
                v = self.binop(op[0], self.eval(left, env), v)
            self.store(left, v, env)
            return v
        if k == "update":
            old = tonum(self.eval(e[3], env))
            new = norm(old + 1 if e[1] == "++" else old - 1)
            self.store(e[3], new, env)
            return new if e[2] else old
        if k == "cond":
            return self.eval(e[2] if truthy(self.eval(e[1], env)) else e[3], env)
        if k == "seq":
            v = UNDEF
            for x in e[1]:
                v = self.eval(x, env)
            return v
        if k == "unary":
            op = e[1]
            if op == "typeof":
                if e[2][0] == "name" and env.lookup(e[2][1]) is None:
                    return "undefined"
                return typeof(un(self.eval(e[2], env)))
            v = self.eval(e[2], env)
            if op == "!":
                # A tracked boolean stays tracked, its prop marked negated.
                if isinstance(v, Tracked) and isinstance(v.value, bool):
                    prop = v.prop[1:] if v.prop.startswith("!") else "!" + v.prop
                    return Tracked(not v.value, v.origin, prop)
                return not truthy(v)
            if op == "-":
                return BigInt(-un(v).v) if isinstance(un(v), BigInt) else norm(-tonum(v))
            if op == "+":
                return tonum(v)
            if op == "void":
                return UNDEF
            raise Unsupported("unary " + op)
        if k == "bin":
            op = e[1]
            if op == "&&":
                a = self.eval(e[2], env)
                return self.eval(e[3], env) if truthy(a) else a
            if op == "||":
                a = self.eval(e[2], env)
                return a if truthy(a) else self.eval(e[3], env)
            if op == "??":
                a = self.eval(e[2], env)
                return self.eval(e[3], env) if un(a) in (UNDEF, NULL) else a
            return self.binop(op, self.eval(e[2], env), self.eval(e[3], env))
        raise Unsupported("expression " + k)

    def store(self, target, v, env):
        if target[0] == "name":
            env.set(target[1], v)
        elif target[0] == "member":
            obj = un(self.eval(target[1], env))
            key = un(self.eval(target[2], env))
            if isinstance(obj, JSArray) and isinstance(key, (int, float)):
                i = int(key)
                while len(obj.items) <= i:
                    obj.items.append(UNDEF)
                obj.items[i] = v
            elif isinstance(obj, JSObject):
                obj.set(jsstr(key), v)
            else:
                raise Unsupported("store into %r" % (obj,))
        elif target[0] == "array":
            items = self.iterate(un(v))
            for i, t in enumerate(target[1]):
                if t[0] == "lit" and t[1] is UNDEF:
                    continue
                self.store(t, items[i] if i < len(items) else UNDEF, env)
        else:
            raise Unsupported("store " + target[0])

    def binop(self, op, a, b):
        a, b = un(a), un(b)
        if (isinstance(a, BigInt) or isinstance(b, BigInt)) and op not in ("===", "!==", "==", "!=", "instanceof", "in"):
            if op == "+" and (isinstance(a, str) or isinstance(b, str)):
                return jsstr(a) + jsstr(b)
            return bigint_op(op, a, b)
        if op == "===":
            return strict_equal(a, b)
        if op == "!==":
            return not strict_equal(a, b)
        if op in ("==", "!="):
            if (a in (UNDEF, NULL)) or (b in (UNDEF, NULL)):
                r = (a in (UNDEF, NULL)) and (b in (UNDEF, NULL))
            elif type(a) is type(b) or (isinstance(a, (int, float)) and isinstance(b, (int, float))):
                r = strict_equal(a, b)
            else:
                raise Unsupported("loose equality")
            return r if op == "==" else not r
        if op == "+":
            if isinstance(a, str) or isinstance(b, str) or isinstance(a, (JSObject, JSArray)) or hasattr(a, "js_string") or hasattr(b, "js_string"):
                return jsstr(a) + jsstr(b)
            return norm(tonum(a) + tonum(b))
        if op in ("<", ">", "<=", ">="):
            if isinstance(a, str) and isinstance(b, str):
                x, y = a, b
            else:
                x, y = tonum(a), tonum(b)
            return {"<": x < y, ">": x > y, "<=": x <= y, ">=": x >= y}[op]
        if op == "instanceof":
            return instance_of(a, b)
        if op == "in":
            if isinstance(b, JSObject):
                return jsstr(a) in b.props
            raise Unsupported("in")
        x, y = tonum(a), tonum(b)
        if op == "-":
            return norm(x - y)
        if op == "*":
            return norm(x * y)
        if op == "/":
            if y == 0:
                return math.nan if x == 0 else math.copysign(math.inf, x) * math.copysign(1, y)
            return norm(x / y)
        if op == "%":
            if y == 0:
                return math.nan
            return norm(math.fmod(x, y))
        if op == "**":
            return norm(x ** y)
        if op in ("|", "&", "^"):
            xi, yi = int(x) & 0xFFFFFFFF, int(y) & 0xFFFFFFFF
            r = {"|": xi | yi, "&": xi & yi, "^": xi ^ yi}[op]
            return r - (1 << 32) if r >= 1 << 31 else r
        raise Unsupported("operator " + op)

    def get_member(self, obj, key):
        obj = un(obj) if not isinstance(obj, Tracked) or not isinstance(obj.value, str) else obj.value
        key = un(key)
        if isinstance(obj, JSArray):
            if isinstance(key, (int, float)) and not isinstance(key, bool):
                i = int(key)
                return obj.items[i] if 0 <= i < len(obj.items) else UNDEF
            return array_method(self, obj, key)
        if isinstance(obj, str):
            if isinstance(key, (int, float)):
                i = int(key)
                return obj[i] if 0 <= i < len(obj) else UNDEF
            return string_method(obj, key)
        if isinstance(obj, (int, float)) and not isinstance(obj, bool):
            if key == "toString":
                return Builtin(lambda *a: jsstr(obj))
            raise Unsupported("number member " + str(key))
        if isinstance(obj, JSObject):
            if key == "hasOwnProperty":
                return Builtin(lambda k: jsstr(k) in obj.props)
            return obj.get(jsstr(key))
        if hasattr(obj, "js_get"):
            return obj.js_get(jsstr(key))
        if isinstance(obj, (JSFunction, Builtin)) and key in ("name",):
            return obj.name or ""
        raise Unsupported("member %r of %r" % (key, obj))


def pattern_names(p):
    k = p[0]
    if k == "name":
        return [p[1]]
    if k == "default":
        return pattern_names(p[1])
    if k == "rest":
        return pattern_names(p[1])
    if k == "arraypat":
        return [n for q in p[1] if q is not None for n in pattern_names(q)]
    if k == "objpat":
        return [n for _, q in p[1] for n in pattern_names(q)]
    return []


def typeof(v):
    # A value the interpreter only stands in for answers itself, or refuses.
    own = getattr(v, "js_typeof", None)
    if own is not None:
        return own()
    if isinstance(v, BigInt):
        return "bigint"
    if v is UNDEF:
        return "undefined"
    if v is NULL:
        return "object"
    if isinstance(v, bool):
        return "boolean"
    if isinstance(v, (int, float)):
        return "number"
    if isinstance(v, str):
        return "string"
    if isinstance(v, (JSFunction, Builtin, ErrorClass)) or hasattr(v, "construct"):
        return "function"
    return "object"


def make_error(cls, message):
    err = JSObject({"name": cls.name, "message": jsstr(message)})
    err.error_class = cls
    return err


def instance_of(a, b):
    if isinstance(b, ErrorClass):
        return getattr(a, "error_class", None) is b
    if hasattr(b, "is_instance"):
        return b.is_instance(a)
    raise Unsupported("instanceof")


def array_method(interp, arr, key):
    items = arr.items
    if key == "length":
        return len(items)
    if key == "includes":
        return Builtin(lambda v, *r: any(same_value(x, v) or (strict_equal(x, v)) for x in items))
    if key == "indexOf":
        return Builtin(lambda v, *r: next((i for i, x in enumerate(items) if strict_equal(x, v)), -1))
    if key == "push":
        def push(*vs):
            items.extend(vs)
            return len(items)
        return Builtin(push)
    if key == "forEach":
        def for_each(fn, *r):
            for i, x in enumerate(list(items)):
                interp.call(fn, [x, i, arr])
            return UNDEF
        return Builtin(for_each)
    if key == "map":
        return Builtin(lambda fn, *r: JSArray([interp.call(fn, [x, i, arr]) for i, x in enumerate(list(items))]))
    if key == "filter":
        return Builtin(lambda fn, *r: JSArray([x for i, x in enumerate(list(items)) if truthy(interp.call(fn, [x, i, arr]))]))
    if key == "some":
        return Builtin(lambda fn, *r: any(truthy(interp.call(fn, [x, i, arr])) for i, x in enumerate(list(items))))
    if key == "every":
        return Builtin(lambda fn, *r: all(truthy(interp.call(fn, [x, i, arr])) for i, x in enumerate(list(items))))
    if key == "join":
        return Builtin(lambda sep=",", *r: jsstr(sep).join("" if un(x) in (UNDEF, NULL) else jsstr(x) for x in items))
    if key == "slice":
        def slc(start=0, end=UNDEF):
            s = int(tonum(start))
            e = len(items) if end is UNDEF else int(tonum(end))
            return JSArray(items[s:e] if s >= 0 else items[s:])
        return Builtin(slc)
    if key == "concat":
        def concat(*others):
            out = list(items)
            for o in others:
                out.extend(o.items if isinstance(o, JSArray) else [o])
            return JSArray(out)
        return Builtin(concat)
    if key == "reverse":
        def rev():
            items.reverse()
            return arr
        return Builtin(rev)
    raise Unsupported("array method " + str(key))


def string_method(s, key):
    if key == "length":
        return len(s)
    if key == "padStart":
        return Builtin(lambda n, fill=" ": s.rjust(int(tonum(n)), jsstr(fill)[0]) if len(s) < int(tonum(n)) else s)
    if key == "padEnd":
        return Builtin(lambda n, fill=" ": s.ljust(int(tonum(n)), jsstr(fill)[0]))
    if key == "startsWith":
        return Builtin(lambda p, *r: s.startswith(jsstr(p)))
    if key == "endsWith":
        return Builtin(lambda p, *r: s.endswith(jsstr(p)))
    if key == "includes":
        return Builtin(lambda p, *r: jsstr(p) in s)
    if key == "slice":
        return Builtin(lambda a=0, b=UNDEF: s[int(tonum(a)):(len(s) if b is UNDEF else int(tonum(b)))])
    if key == "toString":
        return Builtin(lambda: s)
    if key in ("toLowerCase", "toUpperCase"):
        if not s.isascii():
            raise Unsupported("case mapping beyond ASCII")
        return Builtin(lambda: s.lower() if key == "toLowerCase" else s.upper())
    if key == "split":
        return Builtin(lambda sep: JSArray(s.split(jsstr(sep)) if sep != "" else list(s)))
    raise Unsupported("string method " + str(key))


def to_bigint(v=UNDEF):
    v = un(v)
    if isinstance(v, BigInt):
        return v
    if isinstance(v, (int, float)) and not isinstance(v, bool) and float(v).is_integer():
        return BigInt(int(v))
    raise Unsupported("BigInt of %r" % (v,))


def standard_globals(interp_ref):
    def object_entries(o):
        o = un(o)
        if isinstance(o, JSObject):
            return JSArray([JSArray([k, v]) for k, v in o.props.items()])
        if isinstance(o, JSArray):
            return JSArray([JSArray([str(i), v]) for i, v in enumerate(o.items)])
        raise Unsupported("entries")

    def object_keys(o):
        o = un(o)
        if isinstance(o, JSObject):
            return JSArray(list(o.props))
        raise Unsupported("keys")

    def object_assign(t, *srcs):
        for s in srcs:
            s = un(s)
            if isinstance(s, JSObject):
                t.props.update(s.props)
        return t

    def from_entries(arr):
        return JSObject({jsstr(p.items[0]): p.items[1] for p in un(arr).items})

    obj = JSObject({
        "entries": Builtin(object_entries),
        "keys": Builtin(object_keys),
        "assign": Builtin(object_assign),
        "fromEntries": Builtin(from_entries),
        "values": Builtin(lambda o: JSArray(list(un(o).props.values()))),
    })
    math_obj = JSObject({
        "floor": Builtin(lambda x: norm(math.floor(tonum(x)))),
        "ceil": Builtin(lambda x: norm(math.ceil(tonum(x)))),
        "abs": Builtin(lambda x: norm(abs(tonum(x)))),
        "trunc": Builtin(lambda x: norm(math.trunc(tonum(x)))),
        "max": Builtin(lambda *xs: norm(max(tonum(x) for x in xs))),
        "min": Builtin(lambda *xs: norm(min(tonum(x) for x in xs))),
        "sign": Builtin(lambda x: norm((tonum(x) > 0) - (tonum(x) < 0))),
    })

    def array_from(src, fn=UNDEF):
        src = un(src)
        if isinstance(src, JSObject) and "length" in src.props:
            items = [UNDEF] * int(tonum(src.get("length")))
        elif isinstance(src, JSArray):
            items = list(src.items)
        else:
            raise Unsupported("Array.from")
        if fn is not UNDEF:
            items = [interp_ref[0].call(fn, [x, i]) for i, x in enumerate(items)]
        return JSArray(items)

    array_obj = JSObject({"from": Builtin(array_from),
                          "isArray": Builtin(lambda v: isinstance(un(v), JSArray))})
    number_obj = CallableObject(lambda v=0, *r: tonum(v), {
        "MAX_SAFE_INTEGER": 2**53 - 1,
        "MIN_SAFE_INTEGER": -(2**53 - 1),
        "EPSILON": 2.0**-52,
        "MAX_VALUE": 1.7976931348623157e308,
        "isInteger": Builtin(lambda v: isinstance(un(v), (int, float)) and not isinstance(un(v), bool) and float(un(v)).is_integer()),
    })
    class SetClass:
        def construct(self, args):
            items = []
            if args and un(args[0]) is not UNDEF:
                for x in interp_ref[0].iterate(un(args[0])):
                    if not any(same_value(x, y) for y in items):
                        items.append(x)
            return JSSet(items)

    return {
        "Set": SetClass(),
        "Object": obj, "Math": math_obj, "Array": array_obj, "Number": number_obj,
        "String": Builtin(jsstr, "String"),
        "BigInt": Builtin(to_bigint, "BigInt"),
        "Infinity": math.inf, "NaN": math.nan,
        "RangeError": ErrorClass("RangeError"), "TypeError": ErrorClass("TypeError"),
        "Error": ErrorClass("Error"), "Test262Error": ErrorClass("Test262Error"),
        "isNaN": Builtin(lambda v: isinstance(tonum(v), float) and math.isnan(tonum(v))),
    }


class JSSet:
    def __init__(self, items):
        self.items = items

    def js_get(self, key):
        if key == "has":
            return Builtin(lambda v: any(same_value(v, x) for x in self.items))
        if key == "add":
            def add(v):
                if not any(same_value(v, x) for x in self.items):
                    self.items.append(v)
                return self
            return Builtin(add)
        if key == "size":
            return len(self.items)
        raise Unsupported("Set." + key)


class CallableObject(JSObject):
    # A built-in that is both a function and a namespace (Number).
    def __init__(self, fn, props):
        super().__init__(props)
        self.fn = fn

    def call(self, args):
        return self.fn(*args)
