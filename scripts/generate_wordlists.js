// @ts-check

import { symbolCombinations, weirdSymbols } from "./wordlists.js";

/**
 * @template T
 * @param {T[]} arr
 * @param {number} n
 * @returns {T[]}
 */
function repeat(arr, n) {
  /** @type {T[]} */
  const newArr = [];
  for (let i = 0; i < n; i++) {
    newArr.push(...arr);
  }
  return newArr;
}

/**
 * @template T
 * @param {T[]} arr
 * @returns {T[]}
 */
function shuffle(arr) {
  return arr
    .map((v) => /** @type {const} */ ([v, Math.random()]))
    .sort((a, b) => a[1] - b[1])
    .map(([v, _]) => v);
}

/**
 * @template T
 * @param {T[][]} arrs
 * @returns {T[][]}
 */
function zip(arrs) {
  /** @type {T[][]} */
  const newArrs = [];
  const length = arrs[0].length;
  for (let i = 0; i < length; i++) {
    /** @type {T[]} */
    const newArr = [];
    for (let j = 0; j < arrs.length; j++) {
      newArr.push(arrs[j][i]);
    }
    newArrs.push(newArr);
  }
  return newArrs;
}

const allSymbols = repeat([...symbolCombinations, ...weirdSymbols], 20);
const combos = zip([shuffle(allSymbols), shuffle(allSymbols)]).map((v) =>
  v.join(""),
);
console.log(combos.join(" "));
