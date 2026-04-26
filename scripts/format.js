// @ts-check

import assert from "node:assert";
import * as fs from "node:fs";
import { argv } from "node:process";

/** @type {Readonly<Config>}  */
const config = {
  spacing: 2,
  trimTrailingWhitespace: true,
  boards: {
    glove80: {
      matrix: [
        [1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1],
        [1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1],
        [1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1],
        [1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1],
        [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
        [1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1],
      ],
      columnSpacings: [0, 0, 0, 0, 0, 2, 0, 0, 4, 0, 0, 2, 0, 0, 0, 0, 0],
    },
  },
};

function verifyConfig() {
  assert(config.spacing >= 1, "Spacing must be at least 1");

  for (const name in config.boards) {
    const board = config.boards[name];
    if (board === undefined) {
      continue;
    }

    const height = board.matrix.length;
    const width = board.matrix[0]?.length;

    for (const row of board.matrix) {
      assert.equal(row.length, width, `Inconsistent matrix width: ${name}`);
    }

    assert.equal(
      board.columnSpacings.length,
      width - 1,
      `Inconsistent matrix column spacing count: ${name}`,
    );

    for (const spacing of board.columnSpacings) {
      assert(spacing >= 0, "Column spacing cannot be negative");
    }
  }
}

class LineQueue {
  /**
   *
   * @param {string[]} lines
   */
  constructor(lines) {
    /** @private @readonly @type {string[]} */
    this.lines = lines;
    /** @private @type {string[]} */
    this.formattedLines = [];
    /** @private @type {number} */
    this.index = 0;
  }
  /**
   *
   * @param  {...string} lines
   */
  push(...lines) {
    this.formattedLines.push(...lines);
    this.index += lines.length;
  }
  /**
   *
   * @returns {string}
   */
  takeCurrent() {
    if (!this.hasLines()) {
      throw new Error("Queue indexed out of bounds");
    }
    return this.lines[this.index];
  }
  /**
   *
   * @param {number} n
   * @returns {string[]}
   */
  takeLines(n) {
    return this.lines.slice(this.index, this.index + n);
  }

  /**
   *
   * @returns {string[]}
   */
  getFormattedLines() {
    return this.formattedLines;
  }

  /**
   * @returns {boolean}
   */
  hasLines() {
    return this.index < this.lines.length;
  }
}

/**
 *
 * @param {string} name
 * @returns {BoardInfo | null}
 */
function getBoardInfo(name) {
  const board = config.boards[name];
  if (board === undefined) {
    return null;
  }

  return {
    name: name,
    height: board.matrix.length,
    width: board.matrix[0]?.length ?? 0,
    board: board,
  };
}

/**
 *
 * @param {string} line
 * @returns {BoardInfo | null}
 */
function parseMarker(line) {
  const match = line.match(/\/\/\s*format\s*\:\s*([\w-_]+)/);
  if (match === null) {
    return null;
  }
  return getBoardInfo(match[1]);
}

/**
 *
 * @param {string} line
 * @returns {MappingRow}
 */
function parseMappingRow(line) {
  //TODO: Maybe improve by ignoring inside of parentheses

  const [start, ...mappings] = line.split("&");

  return {
    start: start,
    mappings: mappings.map((m) => ("&" + m).trimEnd()),
  };
}

/**
 *
 * @param {MappingRow[]} mappingRows
 * @param {BoardInfo} boardInfo
 * @returns {MappingMatrixRow[] | null}
 */
function arrangeMappingRows(mappingRows, boardInfo) {
  /** @type {MappingMatrixRow[]} */
  const mappingMatrixRows = [];

  for (let rowIndex = 0; rowIndex < boardInfo.height; rowIndex++) {
    const matrixRow = boardInfo.board.matrix[rowIndex];
    const mappingRow = mappingRows[rowIndex];

    const matrixEntryCount = matrixRow
      .map((f) => /** @type {number} */ (f === 0 ? 0 : 1))
      .reduce((a, b) => a + b);
    if (matrixEntryCount !== mappingRow.mappings.length) {
      return null;
    }

    /** @type {(string | null)[]} */
    const matrixItems = [];

    let mappingI = 0;
    for (let matrixI = 0; matrixI < matrixRow.length; matrixI++) {
      const matrixFlag = matrixRow[matrixI];
      if (matrixFlag === 1) {
        matrixItems.push(mappingRow.mappings[mappingI]);
        mappingI++;
      } else {
        matrixItems.push(null);
      }
    }

    mappingMatrixRows.push({
      start: mappingRow.start,
      matrixItems: matrixItems,
    });
  }

  return mappingMatrixRows;
}

/**
 *
 * @param {MappingMatrixRow[]} mappingMatrixRows
 * @param {BoardInfo} boardInfo
 * @returns {string[] | null}
 */
function formatMappingMatrixRows(mappingMatrixRows, boardInfo) {
  /** @type {number[]} */
  const columnWidths = [];
  for (let i = 0; i < boardInfo.width - 1; i++) {
    columnWidths.push(
      Math.max(...mappingMatrixRows.map((r) => r.matrixItems[i]?.length ?? 0)),
    );
  }
  const startWidth = Math.max(...mappingMatrixRows.map((r) => r.start.length));

  const formattedRows = mappingMatrixRows.map((row) => {
    /** @type {string[]} */
    const rowStrings = [];

    rowStrings.push(row.start.padEnd(startWidth));

    for (let i = 0; i < boardInfo.width - 1; i++) {
      rowStrings.push(
        (row.matrixItems[i] ?? "").padEnd(
          columnWidths[i] + boardInfo.board.columnSpacings[i] + config.spacing,
        ),
      );
    }

    rowStrings.push(row.matrixItems[boardInfo.width - 1] ?? "");

    return rowStrings.join("").trimEnd();
  });

  return formattedRows;
}

/**
 *
 * @param {string[]} lines
 * @param {BoardInfo} boardInfo
 * @returns {string[] | null}
 */
function formatBoard(lines, boardInfo) {
  const startLineCount = 1;
  const endLineCount = 1;

  if (lines.length !== boardInfo.height + startLineCount + endLineCount) {
    return null;
  }

  const startLines = lines.slice(undefined, startLineCount);
  const boardLines = lines.slice(startLineCount, lines.length - endLineCount);
  const endLines = lines.slice(lines.length - endLineCount);

  const mappingRows = boardLines.map((l) => parseMappingRow(l));
  const mappingMatrixRows = arrangeMappingRows(mappingRows, boardInfo);
  if (mappingMatrixRows === null) {
    return null;
  }
  const formattedBoardLines = formatMappingMatrixRows(
    mappingMatrixRows,
    boardInfo,
  );
  if (formattedBoardLines === null) {
    return null;
  }

  return [...startLines, ...formattedBoardLines, ...endLines];
}

/**
 *
 * @param {string[]} lines
 * @returns {string[]}
 */
function formatLines(lines) {
  const queue = new LineQueue(lines);

  while (queue.hasLines()) {
    const currentLine = queue.takeCurrent();

    queue.push(currentLine);
    const boardInfo = parseMarker(currentLine);

    if (boardInfo !== null) {
      const mapLines = queue.takeLines(boardInfo.height + 2);
      const formattedBoard = formatBoard(mapLines, boardInfo);
      if (formattedBoard !== null) {
        queue.push(...formattedBoard);
      }
    }
  }

  let formattedLines = queue.getFormattedLines();
  if (config.trimTrailingWhitespace) {
    formattedLines = formattedLines.map((l) => l.trimEnd());
  }
  return formattedLines;
}

/**
 *
 * @param {string} path
 */
function formatFile(path) {
  const file = fs.readFileSync(path).toString("utf-8");

  const lines = file.replaceAll("\r\n", "\n").split("\n");
  const formattedLines = formatLines(lines);
  const formattedFile = formattedLines.join("\n");

  fs.writeFileSync(path, formattedFile, { encoding: "utf-8" });
}

function main() {
  const filePaths = argv.slice(2);

  verifyConfig();

  console.log("Formatting files:", filePaths);

  for (const path of filePaths) {
    formatFile(path);
  }

  console.log("Files formatted successfully");
}

main();
