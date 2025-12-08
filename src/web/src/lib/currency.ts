import type { Currency } from './types';

/**
 * Currency symbols map
 * Used across PowerCard, Stats page, and settings
 */
export const CURRENCY_SYMBOLS: Record<Currency, string> = {
  USD: '$',
  EUR: '€',
  GBP: '£',
  AUD: 'A$',
  CAD: 'C$',
  JPY: '¥',
  CHF: 'Fr',
  ILS: '₪',
};

/**
 * Default fallback currency symbol
 */
export const DEFAULT_CURRENCY_SYMBOL = '$';

/**
 * Get currency symbol for a given currency code
 * Falls back to '$' for unknown currencies
 */
export function getCurrencySymbol(currency: Currency | string): string {
  return CURRENCY_SYMBOLS[currency as Currency] || DEFAULT_CURRENCY_SYMBOL;
}

/**
 * Currency options for select dropdowns
 */
export const CURRENCY_OPTIONS = [
  { code: 'USD' as Currency, symbol: '$', name: 'US Dollar' },
  { code: 'EUR' as Currency, symbol: '€', name: 'Euro' },
  { code: 'GBP' as Currency, symbol: '£', name: 'British Pound' },
  { code: 'AUD' as Currency, symbol: 'A$', name: 'Australian Dollar' },
  { code: 'CAD' as Currency, symbol: 'C$', name: 'Canadian Dollar' },
  { code: 'JPY' as Currency, symbol: '¥', name: 'Japanese Yen' },
  { code: 'CHF' as Currency, symbol: 'Fr', name: 'Swiss Franc' },
  { code: 'ILS' as Currency, symbol: '₪', name: 'Israeli Shekel' },
] as const;

