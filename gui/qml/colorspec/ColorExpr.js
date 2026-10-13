.pragma library

// A tiny, safe math language for the "Equation" role of a color group (see ColorHarmony.js): numbers,
// variables, + - * / % ^ (power, also **), comparisons, ?: and the functions of JavaScript's Math with the same
// names (abs, sin, pow, min, max, round …) plus clamp, mix, step, smoothstep, mod, deg, rad. Constants PI, E,
// TAU. Nothing is passed to eval(): the text is parsed into a tree and compiled to closures, so a config file
// cannot run code. Also used to colour the editor (tokens()).

var FUNCS = {
    abs: Math.abs, sign: Math.sign, floor: Math.floor, ceil: Math.ceil, round: Math.round, trunc: Math.trunc,
    sqrt: Math.sqrt, cbrt: Math.cbrt, exp: Math.exp, log: Math.log, log2: Math.log2, log10: Math.log10,
    sin: Math.sin, cos: Math.cos, tan: Math.tan, asin: Math.asin, acos: Math.acos, atan: Math.atan,
    atan2: Math.atan2, sinh: Math.sinh, cosh: Math.cosh, tanh: Math.tanh, pow: Math.pow, hypot: Math.hypot,
    min: Math.min, max: Math.max,
    clamp: function (x, lo, hi) { return Math.max(lo, Math.min(hi, x)); },
    mix: function (a, b, t) { return a + (b - a) * t; },
    lerp: function (a, b, t) { return a + (b - a) * t; },
    step: function (edge, x) { return x < edge ? 0 : 1; },
    smoothstep: function (e0, e1, x) { var t = Math.max(0, Math.min(1, (x - e0) / (e1 - e0))); return t * t * (3 - 2 * t); },
    mod: function (a, b) { return ((a % b) + b) % b; },
    deg: function (r) { return r * 180 / Math.PI; },
    rad: function (d) { return d * Math.PI / 180; }
};
var CONSTS = { PI: Math.PI, E: Math.E, TAU: 2 * Math.PI };

// [{type: num|ident|op|paren|comma|space|error, text, pos}]
function tokens(src) {
    var out = [], s = String(src || ""), i = 0;
    while (i < s.length) {
        var c = s.charAt(i), m, rest = s.substr(i);
        if ((m = rest.match(/^\s+/)))
            out.push({ type: "space", text: m[0], pos: i });
        else if ((m = rest.match(/^(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?/)))
            out.push({ type: "num", text: m[0], pos: i });
        else if ((m = rest.match(/^[A-Za-z_][A-Za-z_0-9]*/)))
            out.push({ type: "ident", text: m[0], pos: i });
        else if ((m = rest.match(/^(\*\*|<=|>=|==|!=|&&|\|\||[-+*\/%^<>?:!])/)))
            out.push({ type: "op", text: m[0], pos: i });
        else if (c === "(" || c === ")")
            out.push({ type: "paren", text: c, pos: i });
        else if (c === ",")
            out.push({ type: "comma", text: c, pos: i });
        else
            out.push({ type: "error", text: c, pos: i });
        i += out[out.length - 1].text.length;
    }
    return out;
}

// Parses `src` with the variable names `vars` → {fn(env) → number} or {error, pos}
function compile(src, vars) {
    var toks = tokens(src).filter(function (t) { return t.type !== "space"; });
    var p = 0;
    function peek() { return toks[p]; }
    function fail(msg, t) { throw { message: msg, pos: t ? t.pos : String(src).length }; }
    function eat(text) {
        var t = toks[p];
        if (!t || t.text !== text)
            fail("expected “" + text + "”", t);
        ++p;
    }
    function primary() {
        var t = toks[p++];
        if (!t)
            fail("unexpected end");
        if (t.type === "num") {
            var v = Number(t.text);
            return function () { return v; };
        }
        if (t.type === "ident") {
            if (peek() && peek().text === "(") {
                var f = FUNCS[t.text];
                if (!f)
                    fail("unknown function “" + t.text + "”", t);
                ++p;
                var args = [];
                if (peek() && peek().text !== ")") {
                    args.push(ternary());
                    while (peek() && peek().type === "comma") { ++p; args.push(ternary()); }
                }
                eat(")");
                return function (env) { return f.apply(null, args.map(function (a) { return a(env); })); };
            }
            if (CONSTS.hasOwnProperty(t.text)) {
                var k = CONSTS[t.text];
                return function () { return k; };
            }
            if (vars.indexOf(t.text) < 0)
                fail("unknown variable “" + t.text + "”", t);
            var name = t.text;
            return function (env) { return env[name]; };
        }
        if (t.text === "(") {
            var e = ternary();
            eat(")");
            return e;
        }
        if (t.text === "-" || t.text === "+" || t.text === "!") {
            var u = power(), op = t.text; // -2^2 = -(2^2)
            return op === "-" ? function (env) { return -u(env); } : op === "!" ? function (env) { return u(env) ? 0 : 1; } : u;
        }
        fail("unexpected “" + t.text + "”", t);
    }
    function unary() { return primary(); }
    function power() {
        var b = unary();
        if (peek() && (peek().text === "^" || peek().text === "**")) {
            ++p;
            var e = power(); // right associative
            return function (env) { return Math.pow(b(env), e(env)); };
        }
        return b;
    }
    function binary(next, ops) {
        return function () {
            var l = next();
            while (peek() && ops[peek().text]) {
                var f = ops[toks[p++].text], r = next(), a = l;
                l = (function (a, r, f) { return function (env) { return f(a(env), r(env)); }; })(a, r, f);
            }
            return l;
        };
    }
    var mul = binary(power, { "*": function (a, b) { return a * b; }, "/": function (a, b) { return a / b; },
                              "%": function (a, b) { return a % b; } });
    var add = binary(mul, { "+": function (a, b) { return a + b; }, "-": function (a, b) { return a - b; } });
    var cmp = binary(add, { "<": function (a, b) { return a < b ? 1 : 0; }, ">": function (a, b) { return a > b ? 1 : 0; },
                            "<=": function (a, b) { return a <= b ? 1 : 0; }, ">=": function (a, b) { return a >= b ? 1 : 0; },
                            "==": function (a, b) { return a === b ? 1 : 0; }, "!=": function (a, b) { return a !== b ? 1 : 0; } });
    var and = binary(cmp, { "&&": function (a, b) { return a && b ? 1 : 0; } });
    var or = binary(and, { "||": function (a, b) { return a || b ? 1 : 0; } });
    function ternary() {
        var c = or();
        if (peek() && peek().text === "?") {
            ++p;
            var a = ternary();
            eat(":");
            var b = ternary();
            return function (env) { return c(env) ? a(env) : b(env); };
        }
        return c;
    }
    try {
        if (!toks.length)
            fail("empty");
        var bad = toks.filter(function (t) { return t.type === "error"; })[0];
        if (bad)
            fail("unexpected “" + bad.text + "”", bad);
        var fn = ternary();
        if (p < toks.length)
            fail("unexpected “" + toks[p].text + "”", toks[p]);
        return { fn: fn };
    } catch (e) {
        return { error: e.message || String(e), pos: e.pos === undefined ? 0 : e.pos };
    }
}

var cache = {};
// Value of `src` for the variables in `env` → {value} or {error, pos}; compiled trees are cached
function evaluate(src, env) {
    var vars = Object.keys(env);
    var key = vars.join(",") + "\u0001" + src;
    var c = cache[key] || (cache[key] = compile(src, vars));
    if (c.error)
        return c;
    var v = c.fn(env);
    return isFinite(v) ? { value: v } : { error: "not a number", pos: 0 };
}

// Kind of each identifier, for syntax colouring: "var", "func", "const" or "unknown"
function identKind(name, vars) {
    if (vars.indexOf(name) >= 0) return "var";
    if (FUNCS.hasOwnProperty(name)) return "func";
    if (CONSTS.hasOwnProperty(name)) return "const";
    return "unknown";
}

function functionNames() {
    return Object.keys(FUNCS);
}
