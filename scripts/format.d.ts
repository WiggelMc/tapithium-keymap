type BinaryFlag = 0 | 1;

type Config = {
  spacing: number;
  trimTrailingWhitespace: boolean;
  boards: BoardMap;
};

type BoardMap = {
  [key: string]: Board | undefined;
};

type Board = {
  matrix: BinaryFlag[][];
  columnSpacings: number[];
};

type MappingRow = {
  start: string;
  mappings: string[];
};

type MappingMatrixRow = {
  start: string;
  matrixItems: (string | null)[];
};

type BoardInfo = {
  name: string;
  width: number;
  height: number;
  board: Board;
};
