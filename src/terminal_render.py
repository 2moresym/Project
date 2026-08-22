"""Small dependency-free renderer for terminal-friendly AI output."""
from __future__ import annotations
import re

SUPERS = str.maketrans("0123456789+-=()n", "⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿ")
SUBS = str.maketrans("0123456789+-=()", "₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎")


def _math_to_unicode(text: str) -> str:
    # Remove common display-math delimiters.
    text = re.sub(r"\\\[|\\\]|\\\(|\\\)|\$\$", "", text)
    # Common LaTeX operators/symbols.
    replacements = {
        r"\\sqrt": "√",
        r"\\pi": "π",
        r"\\infty": "∞",
        r"\\leq": "≤", r"\\le": "≤",
        r"\\geq": "≥", r"\\ge": "≥",
        r"\\neq": "≠", r"\\pm": "±",
        r"\\times": "×", r"\\cdot": "·",
        r"\\div": "÷", r"\\approx": "≈",
        r"\\rightarrow": "→", r"\\to": "→",
        r"\\left": "", r"\\right": "",
    }
    for pattern, value in replacements.items():
        text = re.sub(pattern, value, text)

    # \frac{a}{b} -> (a)/(b), keeping it readable in a proportional terminal.
    previous = None
    while previous != text:
        previous = text
        text = re.sub(r"\\frac\{([^{}]+)\}\{([^{}]+)\}", r"(\1)/(\2)", text)
        text = re.sub(r"\^\{([^{}]+)\}", lambda m: m.group(1).translate(SUPERS), text)
        text = re.sub(r"_\{([^{}]+)\}", lambda m: m.group(1).translate(SUBS), text)

    # Single-character powers/subscripts: x^2 -> x², x_n -> xₙ where possible.
    text = re.sub(r"\^([0-9+\-=()n])", lambda m: m.group(1).translate(SUPERS), text)
    text = re.sub(r"_([0-9+\-=()])", lambda m: m.group(1).translate(SUBS), text)
    text = text.replace("{", "").replace("}", "")
    return text


def render(text: str, mode: str = "unicode") -> str:
    """Render common math notation for a capable terminal; leave normal prose alone."""
    if mode != "unicode":
        return text

    # Only transform lines that look like LaTeX/math to avoid mangling prose.
    output = []
    for line in text.splitlines():
        looks_math = (
            "\\" in line or "$" in line or
            bool(re.search(r"(?:\^\{.*?\}|_[{A-Za-z0-9]|\\sqrt|\\frac)", line))
        )
        output.append(_math_to_unicode(line) if looks_math else line)
    return "\n".join(output)
