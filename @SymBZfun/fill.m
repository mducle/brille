function fill(obj,varargin)

% First, refill the BZGrid(s) if the hashes differ
newHash = symbz.DataHash(varargin);
if ~strcmp(obj.parameterHash, newHash)
    vecs = obj.get_mapped();
    fillwith = cell(1,obj.nFill);
    [fillwith{:}] = obj.filler{1}(vecs{:},varargin{:});
    for i=2:obj.nFillers
        [fillwith{:}] = obj.filler{i}(fillwith{:});
    end
    
    % reshape the output(s) if necessary
    num = numel(vecs{1});
    for i=1:obj.nFill
        fws = size(fillwith{i});
        obj.shape{i} = fws(2:end);
        if length(obj.shape{i}) > 1
            fillwith{i} = reshape( fillwith{i}, [num, prod(obj.shape{i})] );
        end
    end
    % and smash them together for input into the grid:
    fillwith = cat(2, fillwith{:});
    % 
    assert( numel(fillwith) == num * sum(cellfun(@prod,obj.shape)) )
    % 
    % and finally put them in:
    obj.BZGrid.fill( symbz.m2p(fillwith) );
    
    % we have successfully filled the BZGrid(s), so store the hash.
    obj.parameterHash = newHash;
end

end