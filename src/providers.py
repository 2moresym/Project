import json
import os
import socket
import time
from dataclasses import dataclass
from typing import Protocol, Iterator
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
from .config import DEFAULT_MODEL, DEFAULT_USER_AGENT, HF_ENDPOINT


class AIProvider(Protocol):
    def reply(self, messages: list[dict[str, str]]) -> str: ...
    def stream_reply(self, messages: list[dict[str, str]]) -> Iterator[str]: ...


@dataclass
class DemoProvider:
    def reply(self, messages):
        user = messages[-1].get("content", "").strip() if messages else ""
        if user.lower() in {"hello", "hi", "hey"}:
            return "Hey! I'm the tiny AI playground."
        if "who are you" in user.lower():
            return "I'm a small Python AI playground running in your terminal."
        return f"Demo backend received: {user}\n\nSet HF_TOKEN to use a real model."

    def stream_reply(self, messages):
        yield self.reply(messages)


@dataclass
class OpenAICompatibleProvider:
    token: str
    endpoint: str
    model: str
    user_agent: str = DEFAULT_USER_AGENT
    retries: int = 2

    def _request(self, messages, stream=False):
        payload = json.dumps({
            "model": self.model,
            "messages": messages,
            "temperature": 0.7,
            "max_tokens": 512,
            "stream": stream,
        }).encode()
        req = Request(
            self.endpoint,
            data=payload,
            headers={
                "Authorization": f"Bearer {self.token}",
                "Content-Type": "application/json",
                "Accept": "text/event-stream" if stream else "application/json",
                "User-Agent": self.user_agent,
            },
            method="POST",
        )
        last_error = None
        for attempt in range(self.retries + 1):
            try:
                return urlopen(req, timeout=120)
            except HTTPError as exc:
                detail = exc.read().decode(errors="replace")
                # Authentication, permission, bad requests, and rate limits should
                # be shown immediately; retrying them just wastes time/tokens.
                if exc.code not in {408, 429} and exc.code < 500:
                    raise RuntimeError(f"API HTTP {exc.code}: {detail}") from exc
                last_error = RuntimeError(f"API HTTP {exc.code}: {detail}")
            except URLError as exc:
                reason = exc.reason
                if isinstance(reason, socket.gaierror):
                    message = "DNS lookup failed — check your internet connection or DNS settings."
                elif isinstance(reason, TimeoutError):
                    message = "The AI provider timed out while connecting."
                else:
                    message = f"Network error: {reason}"
                last_error = RuntimeError(message)
            except socket.timeout as exc:
                last_error = RuntimeError("The AI provider timed out while connecting.")
            except OSError as exc:
                last_error = RuntimeError(f"Network error: {exc}")

            if attempt < self.retries:
                time.sleep(0.5 * (2 ** attempt))

        raise last_error or RuntimeError("Unable to contact the AI provider.")

    def reply(self, messages):
        with self._request(messages) as response:
            data = json.loads(response.read().decode())
        try:
            content = data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise RuntimeError(f"Unexpected API response: {data}") from exc
        if not isinstance(content, str):
            raise RuntimeError(f"Unexpected API response: {data}")
        return content.strip()

    def stream_reply(self, messages):
        with self._request(messages, True) as response:
            for raw in response:
                line = raw.decode("utf-8", errors="replace").strip()
                if not line.startswith("data:"):
                    continue
                payload = line[5:].strip()
                if payload == "[DONE]":
                    break
                try:
                    data = json.loads(payload)
                except json.JSONDecodeError:
                    continue
                try:
                    piece = data["choices"][0].get("delta", {}).get("content", "")
                except (KeyError, IndexError, TypeError):
                    piece = ""
                if piece:
                    yield piece


def make_provider(model=None, provider="huggingface") -> AIProvider:
    ua = os.environ.get("HF_USER_AGENT", DEFAULT_USER_AGENT).strip() or DEFAULT_USER_AGENT
    if provider == "openai":
        token = os.environ.get("OPENAI_API_KEY", "").strip()
        endpoint = os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1/chat/completions").strip()
        selected = (os.environ.get("OPENAI_MODEL", model or "gpt-4o-mini")).strip()
        if token:
            return OpenAICompatibleProvider(token, endpoint, selected, ua)
    token = os.environ.get("HF_TOKEN", "").strip()
    selected = (model or os.environ.get("HF_MODEL", DEFAULT_MODEL)).strip() or DEFAULT_MODEL
    return OpenAICompatibleProvider(token, HF_ENDPOINT, selected, ua) if token else DemoProvider()
