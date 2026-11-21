
import argparse
import json
import re
import sys
from pathlib import Path
import subprocess

FLAGS = {
    "IGNORECASE": re.IGNORECASE,
    "MULTILINE": re.MULTILINE,
    "DOTALL": re.DOTALL,
    "ASCII": re.ASCII
}

def normalize_title(title):
    return " ".join(title.split()).casefold()


def load_previous_titles(history_ref, limit=50):
    result = subprocess.run(
        [
            "git",
            "log",
            "--first-parent",
            f"-n{limit}",
            "--format=%s",
            history_ref
        ],
        check=True,
        capture_output=True,
        text=True
    )

    return result.stdout.splitlines()

def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def load_rules(path):
    config_path = Path(path).resolve()
    config = load_json(config_path)

    rules = []

    for rule in config["rules"]:
        rule = rule.copy()
        rule_type = rule.get("type", "regex")

        flags = 0
        for flag in rule.get("flags", []):
            if flag not in FLAGS:
                raise ValueError(f"Unknown regex flag: {flag}")
            flags |= FLAGS[flag]

        if rule_type == "regex":
            rule["_compiled"] = re.compile(rule["pattern"], flags)

        elif rule_type == "word_list":
            word_file = config_path.parent / rule["file"]
            words = load_json(word_file)

            rule["_words"] = [
                re.compile(r"\b" + re.escape(word) + r"\b", flags)
                for word in words
            ]

        elif rule_type not in (
                "alphanumeric_even",
                "palindrome",
                "digit_sum",
                "unique_commit_title",
                "capitalization_limit"
            ):
            raise ValueError(f"Unknown rule type: {rule_type}")

        rules.append(rule)

    return rules


def validate_rule(message, rule, previous_titles=None):
    rule_type = rule.get("type", "regex")

    if rule_type == "regex":
        return bool(rule["_compiled"].search(message))

    if rule_type == "alphanumeric_even":
        count = sum(c.isalnum() for c in message)
        return count % 2 == 0

    if rule_type == "word_list":
        matches = sum(
            bool(pattern.search(message))
            for pattern in rule["_words"]
        )

        return matches >= rule.get("min_matches", 1)

    if rule_type == "palindrome":
        min_length = rule.get("min_length", 5)

        words = re.findall(r"[a-zA-Z]+", message)

        return any(
            len(word) >= min_length and word.lower() == word.lower()[::-1]
            for word in words
        )

    if rule_type == "digit_sum":
        total = sum(int(digit) for digit in re.findall(r"\d", message))
        return total == rule.get("target", 25)

    if rule_type == "unique_commit_title":
        lines = message.splitlines()
        title = normalize_title(lines[0]) if lines else ""

        previous = {
            normalize_title(t)
            for t in (previous_titles or [])[:rule.get("limit", 50)]
        }

        return title not in previous

    if rule_type == "capitalization_limit":
        letters = [c for c in message if c.isalpha()]
    
        if not letters:
            return False
    
        uppercase = sum(c.isupper() for c in letters)
        maximum = int(len(letters) * rule.get("max_percentage", 75) / 100)
    
        return uppercase <= maximum

    return False


def validate(message, rules, previous_titles=None):
    for rule in rules:
        if not validate_rule(message, rule, previous_titles):
            return [(rule["name"], rule["message"])]

    return []



def main():
    parser = argparse.ArgumentParser(
        description="Validate a commit message against configured rules."
    )

    parser.add_argument("-m", "--message")
    parser.add_argument("--stdin", action="store_true")
    SCRIPT_DIR = Path(__file__).resolve().parent

    parser.add_argument(
    "-c", "--config",
    default=str(SCRIPT_DIR / "commit-rules.json"))
    parser.add_argument(
    "--history-ref",
    default="HEAD",
    help="Git reference from which to inspect previous commit titles")

    args = parser.parse_args()

    if args.message is not None:
        message = args.message
    elif args.stdin:
        message = sys.stdin.read()
    else:
        parser.error("Provide --message or --stdin")

    try:
        rules = load_rules(args.config)

        unique_rules = [
            rule for rule in rules
            if rule.get("type") == "unique_commit_title"
        ]

        previous_titles = []

        if unique_rules:
            limit = max(rule.get("limit", 50) for rule in unique_rules)
            previous_titles = load_previous_titles(args.history_ref, limit)

        errors = validate(message, rules, previous_titles)

    except (OSError, ValueError, KeyError, json.JSONDecodeError,
            re.error, subprocess.CalledProcessError) as e:
        print(f"Configuration error: {e}", file=sys.stderr)
        return 2

    if errors:
        name, error_message = errors[0]
        print(f"::error title=Commit lint failed ({name})::{error_message}")
        print(f"  [{name}] {error_message}")
        return 1

    print("::notice title=Commit lint passed::Commit message passed validation.")
    print("Commit message passed validation.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
