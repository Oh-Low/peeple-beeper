import { PAGE_BODY_MAX } from "./hal";

export const CANNED_DEFAULTS = [
  "On my way",
  "Running late",
  "Yes",
  "No",
  "Where are you?",
  "Call me",
] as const;

/** Favorites saved with the heart. Newest first. */
export const CANNED_SAVED_MAX = 16;

export type CannedBook = {
  saved: string[];
};

export function createCannedBook(): CannedBook {
  return { saved: [] };
}

export function cannedReset(book: CannedBook): void {
  book.saved.length = 0;
}

function clipBody(body: string): string {
  return body.replace(/\s+$/g, "").slice(0, PAGE_BODY_MAX);
}

/** Hearted messages first (newest first), then the stock presets. */
export function cannedList(book: CannedBook): string[] {
  return [...book.saved, ...CANNED_DEFAULTS];
}

export function cannedAdd(book: CannedBook, body: string): boolean {
  const text = clipBody(body);
  if (text.length === 0) return false;

  for (const row of CANNED_DEFAULTS) {
    if (row === text) return true;
  }
  const i = book.saved.indexOf(text);
  if (i >= 0) {
    book.saved.splice(i, 1);
  } else if (book.saved.length >= CANNED_SAVED_MAX) {
    book.saved.pop();
  }
  book.saved.unshift(text);
  return true;
}
