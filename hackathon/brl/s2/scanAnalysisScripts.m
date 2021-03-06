% Data analysis for smartscope 2 
% starting 2016.03.28

% 1. Data Entry



%   only a subset of data is worth analyzing for s2 performance.  
%  these data will be copied into a special directory, /local2/s2Data/  and
%  organized into directories for each cell imaged.

%  the post-PV files are stored on my workstation, including .xml files transfered
%  from the rig machine.
batchTopDirectory = '/local2/'
batch1 = dir([batchTopDirectory,'2016*Su*'])% images collected on Sunday.
batch1 = batch1([batch1(:).isdir]')
batch2 = dir([batchTopDirectory,'2016_03_29*'])% images collected on Sunday.
batch2 = batch2([batch2(:).isdir]')
batchTopDirectory2 = '/local2/data/'
batch3 = dir([batchTopDirectory2,'2016_04_04_Mon_10*'])% images of first cell collected on MOnday.
batch3 = [batch3;dir([batchTopDirectory2,'2016_04_04_Mon_11*'])]
    batch3 = [batch3;dir([batchTopDirectory2,'2016_04_04_Mon_12*'])]

    batch3 = [batch3;dir([batchTopDirectory2,'2016_04_04_Mon_13*'])]
    batch3 = batch3([batch3(:).isdir]')

batch4 = dir([batchTopDirectory2,'2016_04_04_Mon_15_*'])% images of second cell collected on MOnday.
batch4 = [batch4; dir([batchTopDirectory2,'2016_04_04_Mon_16_*'])]
batch4 = batch4([batch4(:).isdir]')
batch5 = dir([batchTopDirectory2,'2016_04_05_Tue_*'])% images collected on first part of Tuesday are the same as second cell Monday.
batch5 = batch5([batch5(:).isdir]')
batch12 = numel(batch2)+numel(batch1)
batches = [batch1;batch2;batch3; batch4; batch5]
tic

for i = 1:numel(batches)
    if i <=batch12
        iBatchTopDir = batchTopDirectory
    else
        iBatchTopDir = batchTopDirectory2
    end
    
    if sum(findstr(batches(i).name, '.'))==1
        continue
    end
   sessioniDir = dir([iBatchTopDir,'/', batches(i).name])

    sessioniDir = sessioniDir([sessioniDir(:).isdir]')  % just get scan directories
   for j = 1:numel(sessioniDir)  %  all scans, even aborted ones
        scanjDir = dir([iBatchTopDir, '/', batches(i).name,'/', sessioniDir(j).name,'/','*.xml'])
        if numel(scanjDir)>0
            sessionData{i}(j) = scanDataFromXMLDir([iBatchTopDir, '/', batches(i).name,'/', sessioniDir(j).name]);
            
        end
        
   end
   

end
toc
% 914s for 25 sessions






%% 2.  Analysis

% determine difference between summed tile volume and the actual scanned
% volume [estimate from (whole micron?) binary images] 



% for a set of tile locations, build a binary array to model the scan area:

for i = 1:numel(sessionData)
    for j = 1:numel(sessionData{i})
        
        tileSetij = sessionData{i}(j).tileLocations;
        nTiles = numel(tileSetij)
        sessionData{i}(j).ignore = false;
        
        
        if ( nTiles<3 )|| isempty(sessionData{i}(j).folderName) || sum(strfind(sessionData{i}(j).folderName, '.'))>0
            sessionData{i}(j).ignore = true;
            continue
        else
            ijdir = dir(sessionData{i}(j).folderName)
            isGrid = false;
            for s = 1:numel(ijdir)
                isGrid = isGrid || (sum(strfind( ijdir(s).name, 'Grid'))>0);
            end
            sessionData{i}(j).isGridScan = isGrid;
            
            if sessionData{i}(j).isGridScan
                sessionData{i}(j).ignore  =true
                'gridscan'
                continue
            end
            allLocations = cell2mat(tileSetij');
            [bigRectx, bigRecty] = meshgrid(floor(min(allLocations(:,1))):ceil(max(allLocations(:,3))),floor(min(allLocations(:,2))):ceil(max(allLocations(:,4))));
            bigRect = false(size(bigRectx));
            for k = 1:nTiles
                bigRect(:) = bigRect(:)| ((bigRectx(:)>allLocations(k,1)) &( bigRectx(:)<=allLocations(k,3) )& (bigRecty(:)>allLocations(k,2)) &( bigRecty(:)<=allLocations(k,4) ));
                
                
            end
            %figure, imshow(bigRect);
            if strfind(sessionData{i}(j).folderName,'2016_03_27_Sun_09' )
                sessionData{i}(j).neuronNumber = 1;
            end
            if strfind(sessionData{i}(j).folderName,'2016_03_27_Sun_10' )  %  this is bad form... I need a cell index that comes from the s2 program.
                sessionData{i}(j).neuronNumber = 2;
            end
            
            if strfind(sessionData{i}(j).folderName,'2016_03_27_Sun_11' )
                sessionData{i}(j).neuronNumber = 3;
            end
            if strfind(sessionData{i}(j).folderName,'2016_03_29_Tue_10' )
                sessionData{i}(j).neuronNumber = 4;
            end
            if strfind(sessionData{i}(j).folderName,'2016_03_29_Tue_13' )
                sessionData{i}(j).neuronNumber = 5;
            end
            if strfind(sessionData{i}(j).folderName,'2016_03_29_Tue_15' )
                sessionData{i}(j).neuronNumber = 6;
            end
            if strfind(sessionData{i}(j).folderName,'2016_03_29_Tue_17' )
                sessionData{i}(j).neuronNumber = 7;
            end
            
            
            if (sum(strfind(sessionData{i}(j).folderName,'2016_04_04_Mon_10' )) >0 )|| (sum(strfind(sessionData{i}(j).folderName,'2016_04_04_Mon_11' )) >0 )|| (sum(strfind(sessionData{i}(j).folderName,'2016_04_04_Mon_12' )) >0 ) || (sum(strfind(sessionData{i}(j).folderName,'2016_04_04_Mon_13' )) >0 )%
                sessionData{i}(j).neuronNumber = 8;
            end
            
            if sum(strfind(sessionData{i}(j).folderName,'2016_04_04_Mon_15' ))>0 || (sum(strfind(sessionData{i}(j).folderName,'2016_04_04_Mon_16' )) >0 ) %
                sessionData{i}(j).neuronNumber = 9;
            end
            
            if sum( strfind(sessionData{i}(j).folderName,'2016_04_05_Tue_15' ))>0 || (sum(strfind(sessionData{i}(j).folderName,'2016_04_05_Tue_14' )) >0 ) %
                sessionData{i}(j).neuronNumber = 9;
            end
            
            if sum(strfind(sessionData{i}(j).folderName,'2016_04_05_Tue_17' ))>0 %
                sessionData{i}(j).neuronNumber = 10;
            end
            sessionData{i}(j).boundingBoxArea = numel(bigRect);
            sessionData{i}(j).imagedArea = sum(bigRect(:));
            sessionData{i}(j).tileAreas = (allLocations(:,4)-allLocations(:,2)).*(allLocations(:,3)-allLocations(:,1));
            sessionData{i}(j).totalTileArea = sum(sessionData{i}(j).tileAreas);
            sessionData{i}(j).extraScanning = sessionData{i}(j).totalTileArea-sessionData{i}(j).imagedArea;
            sessionData{i}(j).boundingBoxSparsity = sessionData{i}(j).totalTileArea/numel(bigRect);
            sessionData{i}(j).lagTimes =  diff(sessionData{i}(j).tileStartTimes)-sessionData{i}(j).allTileTimes(1:end-1);
            sessionData{i}(j).totalTime = sessionData{i}(j).tileStartTimes(end)-sessionData{i}(j).tileStartTimes(1)+sessionData{i}(j).allTileTimes(end)+min(sessionData{i}(j).lagTimes);
            sessionData{i}(j).minTotalTime = sum(sessionData{i}(j).allTileTimes(:)+min(sessionData{i}(j).lagTimes));
            sessionData{i}(j).minImagingOnly = sum(sessionData{i}(j).allTileTimes(:));
            sessionData{i}(j).estimatedMinLag = min( sessionData{i}(j).lagTimes);
            sessionData{i}(j).estimatedTimePerTileArea = mean((sessionData{i}(j).allTileTimes(:)+sessionData{i}(j).estimatedMinLag)./sessionData{i}(j).tileAreas);
            sessionData{i}(j).estimatedGridTime = numel(bigRect)*sessionData{i}(j).estimatedTimePerTileArea;
        end
    end
end



% extract lag times = (difference between sequential tiles) - tiletime.
% this will include convert/load times but should also show initial big lag
% followed by minimal lags in continuous imaging mode. tough to extract necessary from unnecessary
% delays, though.



% extract 'extra' time (difference between tiletime*N and total time)

%  total time vs tile size for each neuron (N  = 3)

%  total volume vs tile size for each neuron (N = 3)
%% plotting
figure
neuronNumbers = []
neuronData={}
timeSummary = {}
for i = 1:numel(sessionData)
    for j = 1:numel(sessionData{i})
                          if isfield(sessionData{i}(j),'neuronNumber')    
                nn  = sessionData{i}(j).neuronNumber;
neuronNumbers = [neuronNumbers; nn];

                          end
    end
end


a = unique(neuronNumbers)
for ii = 1:numel(a)
    neuronData{a(ii)}=[0,0,0,0,0,0,0,0]
                timeSummary{a(ii)} = [0,0,0]
end

                myCmap = colormap(jet(9));
for i = 1:numel(sessionData)
    for j = 1:numel(sessionData{i})
        
        tileSetij = sessionData{i}(j).tileLocations;
        nTiles = numel(tileSetij);
        if sessionData{i}(j).ignore
            continue
        else
            if isfield(sessionData{i}(j),'neuronNumber')    
                nn  = sessionData{i}(j).neuronNumber;

                    neuronData{nn} = [neuronData{nn} ; [mean(sqrt(sessionData{i}(j).tileAreas)),sessionData{i}(j).imagedArea,mean(sqrt(sessionData{i}(j).tileAreas)),sessionData{i}(j).totalTime, sessionData{i}(j).totalTileArea, sessionData{i}(j).boundingBoxArea,i,j]]

                    timeSummary{nn}= [timeSummary{nn}; [sessionData{i}(j).estimatedGridTime,sessionData{i}(j).minTotalTime , sessionData{i}(j).totalTime]/( mean((sessionData{i}(j).allTileTimes(:)+sessionData{i}(j).estimatedMinLag)))];
           

%                     subplot(2,1,1), hold all, plot(mean(sqrt(sessionData{i}(j).tileAreas)),sessionData{i}(j).imagedArea,'o-', 'DisplayName', sessionData{i}(j).folderName, 'color', myCmap(nn,:))
%                     subplot(2,1,2), hold all, plot(mean(sqrt(sessionData{i}(j).tileAreas)),sessionData{i}(j).totalTime, '*-', 'DisplayName', sessionData{i}(j).folderName, 'color', myCmap(nn,:))
%                     plot(mean(sqrt(sessionData{i}(j).tileAreas)),sessionData{i}(j).minTotalTime, '*-','color', myCmap(nn,:))
                end 
            
        end
    end
end
%
a = unique(neuronNumbers)
for ii = 1:numel(a)
neuronData{a(ii)} = neuronData{a(ii)}(neuronData{a(ii)}(:,1)~=0,:)
neuronData{a(ii)} = sortrows(neuronData{a(ii)},1)
    subplot(2,2,1),  hold all,   plot(neuronData{a(ii)}(:,1),neuronData{a(ii)}(:,2), '*-','color', myCmap(ii,:),'DisplayName', sessionData{neuronData{a(ii)}(end,7)}(neuronData{a(ii)}(end,8)).folderName);
subplot(2,2,2),    hold all, plot(neuronData{a(ii)}(:,3),neuronData{a(ii)}(:,4), '*-','color', myCmap(ii,:),'DisplayName',sessionData{neuronData{a(ii)}(end,7)}(neuronData{a(ii)}(end,8)).folderName);
subplot(2,2,3) ,   hold all, plot(neuronData{a(ii)}(:,5),neuronData{a(ii)}(:,6), '*-','color', myCmap(ii,:),'DisplayName',sessionData{neuronData{a(ii)}(end,7)}(neuronData{a(ii)}(end,8)).folderName);
subplot(2,2,4) ,   hold all, plot(neuronData{a(ii)}(:,2),neuronData{a(ii)}(:,6), '*-','color', myCmap(ii,:),'DisplayName',sessionData{neuronData{a(ii)}(end,7)}(neuronData{a(ii)}(end,8)).folderName);

end


%%  notes and comments.

%  cell000 neuronNumber = 1
%  2016_03_27_Sun_09_45  background was too high. but the tile size is the
%  same as 9_46.  for some reason some preview files got put in the data
%  directory, causing an overestimation of the tile size (which is taken
%  from the average of the tile sizes generated from the xml files)  9_45
%  and the problem tiles in 9_46 were removed to ./other/


% cell001 neuronNumber = 8

% cell002 neuronNumber = 9





% FORMER NAMES! stored in /data
% cell001  neuronNumber = 3 removed looks like crap. MOST tracing, discontinuous tile boundaries,
% poor labeling.

% cell002 (neuronNumber = 4)  THESE ARE ALL TEST SCANS OF NOISE




%%   notes and comments



%  A. /local2/2016_03_27_Sun_09_00/2016_03_27_Sun_10_18  ran away and kept
%  imaging forever.  initial threshold was 10,  189 slices 
% B. /local2/2016_03_27_Sun_09_00/2016_03_27_Sun_10_50   threshold was 15,  169 slices 
%  C. /local2/2016_03_27_Sun_09_00/2016_03_27_Sun_10_55   threshold was 10.  169 slices 
% imaging power was the same for all, but clear drop in fluorescence from
% A. to B.

%10_11 power 100
%10_14 power 110
%10_18-10_55 power 130

%  I loaded the center tile of all 5 scans of this neuron into FIJI and
%  generated the mean value of each slice. those data were loaded into
%  matlab as matrices 

% % plot the peak values of each:
% 
% peakData = [max(mean4022(:,2)), max(mean4026(:,2)),max(mean4036(:,2)),max(mean4159(:,2)),max(mean4170(:,2))]
% 
% backgroundData = [max(mean4022(1:50,2)), max(mean4026(1:50,2)),max(mean4036(1:50,2)),max(mean4159(1:50,2)),max(mean4170(1:50,2))]
% 
% plot(peakData), hold all, plot(backgroundData)
% 
% 
% %  very clear from looking at the data again that tile-to-tile stitching is
%  incorrect. specifically, it looks like the left and right edges of the
%  tiles are not correctly located in space.  we think we have 10-15%
%  overlap, but the image data is NOT actually overlapped.  since the field
%  size is taken directly from PV, it's hard to see how this can be
%  happening.  either the field size is wrong or the distance metric varies
%  across the field along the fastscan axis.  

%