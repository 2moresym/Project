"""Dependency-free Markdown and math renderer for terminal output."""
from __future__ import annotations

import re

SUPERS = str.maketrans("0123456789+-=()n", "⁰¹²³⁴⁵⁶⁷⁸⁹⁺⁻⁼⁽⁾ⁿ")
SUBS = str.maketrans("0123456789+-=()abcdefghijklmnopqrstuvwxyz", "₀₁₂₃₄₅₆₇₈₉₊₋₌₍₎ₐᵦ𝒸ᵈₑ𝒻𝑔ₕᵢⱼₖₗₘₙₒₚᵩᵣₛₜᵤᵥ𝓌ₓᵧ𝓏")

SYMBOLS = {
    r"\pi": "π", r"\infty": "∞", r"\leq": "≤", r"\le": "≤",
    r"\geq": "≥", r"\ge": "≥", r"\neq": "≠", r"\pm": "±",
    r"\times": "×", r"\cdot": "·", r"\div": "÷", r"\approx": "≈",
    r"\equiv": "≡", r"\propto": "∝", r"\sum": "Σ", r"\prod": "Π",
    r"\Delta": "Δ", r"\delta": "δ", r"\theta": "θ", r"\lambda": "λ",
    r"\alpha": "α", r"\beta": "β", r"\gamma": "γ", r"\omega": "ω",
    r"\rightarrow": "→", r"\to": "→", r"\leftarrow": "←",
    r"\Rightarrow": "⇒", r"\therefore": "∴", r"\degree": "°",
}


def _balanced_group(text: str, start: int) -> tuple[str, int] | None:
    if start >= len(text) or text[start] != "{":
        return None
    depth = 0
    for index in range(start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:index], index + 1
    return None


def _replace_frac(text: str) -> str:
    pattern = re.compile(r"\\frac\s*\{")
    while True:
        match = pattern.search(text)
        if not match:
            return text
        numerator = _balanced_group(text, match.end() - 1)
        if not numerator:
            return text
        denominator_start = numerator[1]
        while denominator_start < len(text) and text[denominator_start].isspace():
            denominator_start += 1
        denominator = _balanced_group(text, denominator_start)
        if not denominator:
            return text
        replacement = f"({_math_to_unicode(numerator[0])})/({_math_to_unicode(denominator[0])})"
        text = text[:match.start()] + replacement + text[denominator[1]:]


def _math_to_unicode(text: str) -> str:
    # Normalize common malformed closing delimiters emitted by models, e.g. x^{2\).
    text = re.sub(r"\\\)|\\\]", "}", text)
    text = re.sub(r"\\\(|\\\[", "", text)
    text = text.replace("$$", "").replace("$", "")

    text = _replace_frac(text)
    text = re.sub(r"\\sqrt\s*\{([^{}]*)\}", lambda m: f"√({m.group(1)})", text)
    text = re.sub(r"\\sqrt\s*([^\s,.;]+)", lambda m: f"√({m.group(1)})", text)

    for pattern, value in SYMBOLS.items():
        text = text.replace(pattern, value)
    text = text.replace(r"\left", "").replace(r"\right", "")

    # Normalize powers/subscripts before removing braces.
    text = re.sub(r"\^\{([^{}]+)\}", lambda m: m.group(1).translate(SUPERS), text)
    text = re.sub(r"_\{([^{}]+)\}", lambda m: m.group(1).translate(SUBS), text)
    text = re.sub(r"\^([0-9+\-=()n])", lambda m: m.group(1).translate(SUPERS), text)
    text = re.sub(r"_([0-9+\-=()])", lambda m: m.group(1).translate(SUBS), text)

    # A malformed model fragment such as x^{2\) becomes x² after normalization.
    text = re.sub(r"\^\{([^{}]*)", lambda m: m.group(1).translate(SUPERS), text)
    text = text.replace("{", "").replace("}", "")
    text = re.sub(r"\s+", " ", text).strip()
    return text


def _render_math_segments(line: str) -> str:
    # Display and inline delimiters. The fallback also catches LaTeX commands in prose.
    pattern = re.compile(r"(\\\[[\s\S]*?\\\]|\\\([\s\S]*?\\\)|\$[^$]+\$|\$\$[^$]+\$\$)")
    parts = pattern.split(line)
    rendered: list[str] = []
    for part in parts:
        if not part:
            continue
        if pattern.fullmatch(part) or "\\frac" in part or "\\sqrt" in part:
            rendered.append(_math_to_unicode(part))
        else:
            rendered.append(part)
    return "".join(rendered)


def _render_table(lines: list[str]) -> list[str]:
    rows = []
    for line in lines:
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if cells and not all(re.fullmatch(r"\s*:?-{3,}:?\s*", cell) for cell in cells):
            rows.append(cells)
    if not rows:
        return lines
    width = max(len(row) for row in rows)
    rows = [row + [""] * (width - len(row)) for row in rows]
    widths = [max(len(row[i]) for row in rows) for i in range(width)]
    return ["│ " + " │ ".join(row[i].ljust(widths[i]) for i in range(width)) + " │" for row in rows]


def render(text: str, mode: str = "unicode") -> str:
    """Render common Markdown/math for a capable terminal without external packages."""
    if mode != "unicode":
        return text

    lines = text.replace("\r\n", "\n").split("\n")
    output: list[str] = []
    in_code = False
    table_buffer: list[str] = []

    def flush_table() -> None:
        nonlocal table_buffer
        if table_buffer:
            output.extend(_render_table(table_buffer))
            table_buffer = []

    for line in lines:
        if line.strip().startswith("```") or line.strip().startswith("~~~"):
            flush_table()
            in_code = not in_code
            output.append(line)
            continue
        if in_code:
            output.append(line)
            continue

        stripped = line.strip()
        if "|" in stripped and stripped.count("|") >= 2:
            table_buffer.append(line)
            continue
        flush_table()

        line = _render_math_segments(line)
        line = re.sub(r"^\s*#{1,6}\s+", "", line)
        line = re.sub(r"^\s*[-*]\s+", "• ", line)
        line = re.sub(r"^\s*\d+\.\s+", lambda m: m.group(0).lstrip(), line)
        output.append(line.rstrip())

    flush_table()
    return "\n".join(output)
