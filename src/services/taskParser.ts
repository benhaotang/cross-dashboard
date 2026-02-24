export interface TaskDefaults {
  morningHour: number;    // default 8
  afternoonHour: number;  // default 14
  nightHour: number;      // default 21
  defaultHour: number;    // default 10 — bare "today"/"tomorrow"
}

export const DEFAULT_TASK_DEFAULTS: TaskDefaults = {
  morningHour: 8,
  afternoonHour: 14,
  nightHour: 21,
  defaultHour: 10,
};

export interface ParsedTaskInput {
  summary: string;
  priority: number;      // 0=none, 1=high, 5=medium, 9=low
  categories: string[];
  due?: Date;
}

const DAY_NAMES = ['sunday', 'monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday'];

// Time keyword patterns, ordered by specificity (longer matches first)
interface TimePattern {
  regex: RegExp;
  resolve: (now: Date, defaults: TaskDefaults) => Date;
}

function buildTimePatterns(): TimePattern[] {
  return [
    // Tomorrow + time-of-day
    {
      regex: /\btomorrow\s+morning\b/i,
      resolve: (now, d) => addDays(startOfDay(now), 1, d.morningHour),
    },
    {
      regex: /\btomorrow\s+afternoon\b/i,
      resolve: (now, d) => addDays(startOfDay(now), 1, d.afternoonHour),
    },
    {
      regex: /\btomorrow\s+(night|evening)\b/i,
      resolve: (now, d) => addDays(startOfDay(now), 1, d.nightHour),
    },
    // Bare tomorrow
    {
      regex: /\btomorrow\b/i,
      resolve: (now, d) => addDays(startOfDay(now), 1, d.defaultHour),
    },
    // Today + time-of-day
    {
      regex: /\b(today\s+morning|this\s+morning)\b/i,
      resolve: (now, d) => setHour(startOfDay(now), d.morningHour),
    },
    {
      regex: /\b(today\s+afternoon|this\s+afternoon)\b/i,
      resolve: (now, d) => setHour(startOfDay(now), d.afternoonHour),
    },
    {
      regex: /\b(tonight|today\s+(night|evening)|this\s+evening)\b/i,
      resolve: (now, d) => setHour(startOfDay(now), d.nightHour),
    },
    // Bare today
    {
      regex: /\btoday\b/i,
      resolve: (now, d) => setHour(startOfDay(now), d.defaultHour),
    },
    // Next week
    {
      regex: /\bnext\s+week\b/i,
      resolve: (now, d) => addDays(startOfDay(now), 7, d.defaultHour),
    },
    // Weekday names
    ...DAY_NAMES.map((day, index) => ({
      regex: new RegExp(`\\b${day}\\b`, 'i'),
      resolve: (now: Date, d: TaskDefaults) => nextWeekday(now, index, d.defaultHour),
    })),
  ];
}

function startOfDay(date: Date): Date {
  const d = new Date(date);
  d.setHours(0, 0, 0, 0);
  return d;
}

function setHour(date: Date, hour: number): Date {
  const d = new Date(date);
  d.setHours(hour, 0, 0, 0);
  return d;
}

function addDays(date: Date, days: number, hour: number): Date {
  const d = new Date(date);
  d.setDate(d.getDate() + days);
  d.setHours(hour, 0, 0, 0);
  return d;
}

function nextWeekday(now: Date, targetDay: number, hour: number): Date {
  const current = now.getDay();
  let daysAhead = targetDay - current;
  if (daysAhead <= 0) daysAhead += 7; // always next occurrence
  return addDays(startOfDay(now), daysAhead, hour);
}

export function parseTaskInput(input: string, defaults: TaskDefaults = DEFAULT_TASK_DEFAULTS): ParsedTaskInput {
  let text = input;
  let priority = 0;
  const categories: string[] = [];
  let due: Date | undefined;

  // 1. Extract priority — find longest run of ! characters
  const bangMatches = [...text.matchAll(/!+/g)];
  if (bangMatches.length > 0) {
    let longest = bangMatches[0];
    for (const m of bangMatches) {
      if (m[0].length > longest[0].length) longest = m;
    }
    const count = longest[0].length;
    if (count >= 3) priority = 1;       // high
    else if (count === 2) priority = 5; // medium
    else priority = 9;                  // low
    text = text.replace(longest[0], '');
  }

  // 2. Extract #tags
  const tagMatches = [...text.matchAll(/#(\w+)/g)];
  for (const m of tagMatches) {
    categories.push(m[1]);
  }
  text = text.replace(/#\w+/g, '');

  // 3. Extract explicit HH:MM clock time (e.g. "14:30", "9:00", "21:00")
  //    Negative look-around prevents matching digits that are part of larger numbers.
  let explicitHour: number | undefined;
  let explicitMinute: number | undefined;
  const clockMatch = text.match(/(?<!\d)([01]?\d|2[0-3]):([0-5]\d)(?!\d)/);
  if (clockMatch) {
    explicitHour = parseInt(clockMatch[1], 10);
    explicitMinute = parseInt(clockMatch[2], 10);
    text = text.replace(clockMatch[0], '');
  }

  // 4. Extract time keywords (first match wins, longest patterns checked first)
  const now = new Date();
  const patterns = buildTimePatterns();
  for (const pattern of patterns) {
    const match = text.match(pattern.regex);
    if (match) {
      due = pattern.resolve(now, defaults);
      text = text.replace(pattern.regex, '');
      break;
    }
  }

  // 5. Apply explicit HH:MM: overrides the hour set by keyword, or defaults to today
  if (explicitHour !== undefined && explicitMinute !== undefined) {
    if (due) {
      due.setHours(explicitHour, explicitMinute, 0, 0);
    } else {
      due = new Date();
      due.setHours(explicitHour, explicitMinute, 0, 0);
    }
  }

  // 6. Cleanup: collapse whitespace, trim
  const summary = text.replace(/\s+/g, ' ').trim();

  return { summary, priority, categories, due };
}
